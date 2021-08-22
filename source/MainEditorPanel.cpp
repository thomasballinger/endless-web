/* MainEditorPanel.cpp
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "MainEditorPanel.h"

#include "text/alignment.hpp"
#include "Angle.h"
#include "CargoHold.h"
#include "Dialog.h"
#include "FillShader.h"
#include "Flotsam.h"
#include "FogShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "Galaxy.h"
#include "GameData.h"
#include "Government.h"
#include "Information.h"
#include "Interface.h"
#include "LineShader.h"
#include "MapDetailPanel.h"
#include "MapOutfitterPanel.h"
#include "MapShipyardPanel.h"
#include "Mission.h"
#include "MissionPanel.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "PointerShader.h"
#include "Politics.h"
#include "Preferences.h"
#include "Radar.h"
#include "RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteShader.h"
#include "StarField.h"
#include "StellarObject.h"
#include "SystemEditor.h"
#include "System.h"
#include "Trade.h"
#include "UI.h"
#include "Visual.h"

#include "gl_header.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

using namespace std;



MainEditorPanel::MainEditorPanel(PlayerInfo &player, SystemEditor *systemEditor)
	: player(player), systemEditor(systemEditor)
{
	Select(player.GetSystem());
	SetIsFullScreen(true);
	SetInterruptible(false);

	UpdateCache();
}



void MainEditorPanel::Step()
{
	double zoomTarget = Preferences::ViewZoom();
	if(zoom != zoomTarget)
	{
		static const double ZOOM_SPEED = .05;

		// Define zoom speed bounds to prevent asymptotic behavior.
		static const double MAX_SPEED = .05;
		static const double MIN_SPEED = .002;

		double zoomRatio = max(MIN_SPEED, min(MAX_SPEED, abs(log2(zoom) - log2(zoomTarget)) * ZOOM_SPEED));
		if(zoom < zoomTarget)
			zoom = min(zoomTarget, zoom * (1. + zoomRatio));
		else if(zoom > zoomTarget)
			zoom = max(zoomTarget, zoom * (1. / (1. + zoomRatio)));
	}
}



void MainEditorPanel::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT);
	GameData::Background().Draw(center, Point(), zoom);
	draw.Clear(step, zoom);
	batchDraw.Clear(step, zoom);
	draw.SetCenter(center);
	batchDraw.SetCenter(center);

	for(const auto &object : currentSystem->Objects())
		if(object.HasSprite())
		{
			// Don't apply motion blur to very large planets and stars.
			if(object.Width() >= 280.)
				draw.AddUnblurred(object);
			else
				draw.Add(object);
		}

	asteroids.Step(newVisuals, newFlotsam, step);
	asteroids.Draw(draw, center, zoom);
	for(const auto &visual : newVisuals)
		batchDraw.AddVisual(visual);
	for(const auto &floatsam : newFlotsam)
		draw.Add(*floatsam);
	++step;

	if(currentObject)
	{
		Angle a = currentObject->Facing();
		Angle da(360. / 5);

		PointerShader::Bind();
		for(int i = 0; i < 5; ++i)
		{
			PointerShader::Add((currentObject->Position() - center) * zoom, a.Unit(), 12.f, 14.f, -currentObject->Radius() * zoom, Radar::GetColor(Radar::FRIENDLY));
			a += da;
		}
		PointerShader::Unbind();
	}

	RingShader::Draw(-center * zoom, currentSystem->HabitableZone() * 1.25 * zoom, currentSystem->HabitableZone() * .75 * zoom, 1.f, Color(50.f / 255.f, 205.f / 255.f, 50.f / 255.f).Transparent(.1f));

	draw.Draw();
	batchDraw.Draw();
}



bool MainEditorPanel::AllowFastForward() const
{
	return true;
}



const System *MainEditorPanel::Selected() const
{
	return currentSystem;
}



bool MainEditorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(command.Has(Command::MAP) || key == 'd' || key == SDLK_ESCAPE
			|| (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
		GetUI()->Pop(this);
	else if(key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS)
		Preferences::ZoomViewIn();
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		Preferences::ZoomViewOut();
	else
		return false;

	return true;
}



bool MainEditorPanel::Click(int x, int y, int clicks)
{
	// Figure out if a system was clicked on.
	auto click = Point(x, y) / zoom + center;
	if(!currentSystem || !currentSystem->IsValid())
		return false;
	for(const auto &it : currentSystem->Objects())
		if(click.Distance(it.Position()) < it.Radius())
		{
			currentObject = &it;
			moveStellars = true;
			return true;
		}
	moveStellars = false;
	return true;
}



bool MainEditorPanel::Drag(double dx, double dy)
{
	isDragging = true;
	if(moveStellars && currentObject)
		systemEditor->UpdateStellarPosition(*currentObject, Point(dx, dy) / zoom, currentSystem);
	else
		center -= Point(dx, dy) / zoom;

	return true;
}



bool MainEditorPanel::Scroll(double dx, double dy)
{
	if(dy < 0.)
		Preferences::ZoomViewOut();
	else if(dy > 0.)
		Preferences::ZoomViewIn();
	return true;
}



bool MainEditorPanel::Release(int x, int y)
{
	// Figure out if a system was clicked on, but
	// only if we didn't move the systems.
	if(isDragging)
	{
		isDragging = false;
		moveStellars = false;
		return true;
	}

	return true;
}



void MainEditorPanel::Select(const System *system)
{
	if(!system)
		return;
	currentSystem = system;
	UpdateCache();
}



void MainEditorPanel::UpdateCache()
{
	asteroids.Clear();
	for(const System::Asteroid &a : currentSystem->Asteroids())
	{
		// Check whether this is a minable or an ordinary asteroid.
		if(a.Type())
			asteroids.Add(a.Type(), a.Count(), a.Energy(), currentSystem->AsteroidBelt());
		else
			asteroids.Add(a.Name(), a.Count(), a.Energy());
	}
}
