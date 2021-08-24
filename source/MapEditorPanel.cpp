/* MapEditorPanel.cpp
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "MapEditorPanel.h"

#include "text/alignment.hpp"
#include "Angle.h"
#include "CargoHold.h"
#include "Dialog.h"
#include "FillShader.h"
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
#include "RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteShader.h"
#include "StellarObject.h"
#include "SystemEditor.h"
#include "System.h"
#include "Trade.h"
#include "UI.h"

#include "gl_header.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

using namespace std;

namespace {
	// Length in frames of the recentering animation.
	const int RECENTER_TIME = 20;
}



MapEditorPanel::MapEditorPanel(PlayerInfo &player, SystemEditor *systemEditor)
	: player(player), systemEditor(systemEditor),
	playerJumpDistance(System::DEFAULT_NEIGHBOR_DISTANCE)
{
	selectedSystems.push_back(player.GetSystem());
	SetIsFullScreen(true);
	SetInterruptible(false);

	// Find out how far the player is able to jump. The range of the system
	// takes priority over the range of the player's flagship.
	double systemRange = selectedSystems.back()->JumpRange();
	double playerRange = player.Flagship() ? player.Flagship()->JumpRange() : 0.;
	if(systemRange || playerRange)
		playerJumpDistance = systemRange ? systemRange : playerRange;

	CenterOnSystem(true);
	UpdateCache();
}



void MapEditorPanel::Step()
{
	if(recentering > 0)
	{
		double step = (recentering - .5) / RECENTER_TIME;
		// Interpolate with the smoothstep function, 3x^2 - 2x^3. Its derivative
		// gives the fraction of the distance to move at each time step:
		center += recenterVector * (step * (1. - step) * (6. / RECENTER_TIME));
		--recentering;
	}
}



void MapEditorPanel::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT);

	for(const auto &it : GameData::Galaxies())
		SpriteShader::Draw(it.second.GetSprite(), Zoom() * (center + it.second.Position()), Zoom());

	// Draw the "visible range" circle around your current location.
	Color dimColor(.1f, 0.f);
	RingShader::Draw(Zoom() * (selectedSystems.front()->Position() + center),
		(System::DEFAULT_NEIGHBOR_DISTANCE + .5) * Zoom(), (System::DEFAULT_NEIGHBOR_DISTANCE - .5) * Zoom(), dimColor);
	// Draw the jump range circle around your current location if it is different than the
	// visible range.
	if(playerJumpDistance != System::DEFAULT_NEIGHBOR_DISTANCE)
		RingShader::Draw(Zoom() * (selectedSystems.front()->Position() + center),
			(playerJumpDistance + .5) * Zoom(), (playerJumpDistance - .5) * Zoom(), dimColor);

	Color brightColor(.4f, 0.f);
	for(auto &&system : selectedSystems)
		RingShader::Draw(Zoom() * (system->Position() + center),
			11.f, 9.f, brightColor);

	DrawWormholes();
	DrawLinks();
	DrawSystems();
	DrawNames();
}



bool MapEditorPanel::AllowFastForward() const
{
	return true;
}



const System *MapEditorPanel::Selected() const
{
	return selectedSystems.front();
}



bool MapEditorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	const Interface *mapInterface = GameData::Interfaces().Get("map");
	if(command.Has(Command::MAP) || key == 'd' || key == SDLK_ESCAPE
			|| (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
		GetUI()->Pop(this);
	else if(key == 'f')
	{
		GetUI()->Push(new Dialog(
			this, &MapEditorPanel::Find, "Search for:"));
		return true;
	}
	else if(key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS)
		player.SetMapZoom(min(static_cast<int>(mapInterface->GetValue("max zoom")), player.MapZoom() + 1));
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		player.SetMapZoom(max(static_cast<int>(mapInterface->GetValue("min zoom")), player.MapZoom() - 1));
	else
		return false;

	return true;
}



bool MapEditorPanel::Click(int x, int y, int clicks)
{
	// Figure out if a system was clicked on.
	click = Point(x, y) / Zoom() - center;
	for(const auto &it : GameData::Systems())
		if(it.second.IsValid() && click.Distance(it.second.Position()) < 10.)
		{
			if(find(selectedSystems.begin(), selectedSystems.end(), &it.second) != selectedSystems.end())
				moveSystems = true;
			break;
		}
	return true;
}



bool MapEditorPanel::Drag(double dx, double dy)
{
	isDragging = true;
	if(moveSystems)
	{
		for(auto &&system : selectedSystems)
			systemEditor->UpdateSystemPosition(system, Point(dx, dy) / Zoom());
		UpdateCache();
	}
	else
	{
		center += Point(dx, dy) / Zoom();
		recentering = 0;
	}

	return true;
}



bool MapEditorPanel::Scroll(double dx, double dy)
{
	// The mouse should be pointing to the same map position before and after zooming.
	Point mouse = UI::GetMouse();
	Point anchor = mouse / Zoom() - center;
	const Interface *mapInterface = GameData::Interfaces().Get("map");
	if(dy > 0.)
		player.SetMapZoom(min(static_cast<int>(mapInterface->GetValue("max zoom")), player.MapZoom() + 1));
	else if(dy < 0.)
		player.SetMapZoom(max(static_cast<int>(mapInterface->GetValue("min zoom")), player.MapZoom() - 1));

	// Now, Zoom() has changed (unless at one of the limits). But, we still want
	// anchor to be the same, so:
	center = mouse / Zoom() - anchor;
	return true;
}



bool MapEditorPanel::Release(int x, int y)
{
	// Figure out if a system was clicked on, but
	// only if we didn't move the systems.
	if(isDragging)
	{
		isDragging = false;
		moveSystems = false;
		return true;
	}

	click = Point(x, y) / Zoom() - center;
	for(const auto &it : GameData::Systems())
		if(it.second.IsValid() && click.Distance(it.second.Position()) < 10.)
		{
			Select(&it.second);
			break;
		}

	return true;
}



Color MapEditorPanel::MapColor(double value)
{
	if(std::isnan(value))
		return UninhabitedColor();

	value = min(1., max(-1., value));
	if(value < 0.)
		return Color(
			.12 + .12 * value,
			.48 + .36 * value,
			.48 - .12 * value,
			.4);
	else
		return Color(
			.12 + .48 * value,
			.48,
			.48 - .48 * value,
			.4);
}



Color MapEditorPanel::GovernmentColor(const Government *government)
{
	if(!government)
		return UninhabitedColor();

	return Color(
		.6f * government->GetColor().Get()[0],
		.6f * government->GetColor().Get()[1],
		.6f * government->GetColor().Get()[2],
		.4f);
}



Color MapEditorPanel::UninhabitedColor()
{
	return GovernmentColor(GameData::Governments().Get("Uninhabited"));
}



void MapEditorPanel::Select(const System *system)
{
	if(!system)
		return;

	// Pressing shift selects multiple systems.
	if(!(SDL_GetModState() & KMOD_SHIFT))
		selectedSystems.clear();
	selectedSystems.push_back(system);
}



void MapEditorPanel::Find(const string &name)
{
	int bestIndex = 9999;
	for(const auto &it : GameData::Systems())
		if(it.second.IsValid())
		{
			int index = Search(it.first, name);
			if(index >= 0 && index < bestIndex)
			{
				bestIndex = index;
				selectedSystems.clear();
				selectedSystems.push_back(&it.second);
				CenterOnSystem();
				if(!index)
					return;
			}
		}
	for(const auto &it : GameData::Planets())
		if(it.second.IsValid())
		{
			int index = Search(it.first, name);
			if(index >= 0 && index < bestIndex)
			{
				bestIndex = index;
				selectedSystems.clear();
				selectedSystems.push_back(it.second.GetSystem());
				CenterOnSystem();
				if(!index)
					return;
			}
		}
}



double MapEditorPanel::Zoom() const
{
	return pow(1.5, player.MapZoom());
}



int MapEditorPanel::Search(const string &str, const string &sub)
{
	auto it = search(str.begin(), str.end(), sub.begin(), sub.end(),
		[](char a, char b) { return toupper(a) == toupper(b); });
	return (it == str.end() ? -1 : it - str.begin());
}



void MapEditorPanel::CenterOnSystem(bool immediate)
{
	const auto *system = selectedSystems.back();
	if(immediate)
		center = -system->Position();
	else
	{
		recenterVector = -system->Position() - center;
		recentering = RECENTER_TIME;
	}
}



// Cache the map layout, so it doesn't have to be re-calculated every frame.
// The node cache must be updated when the coloring mode changes.
void MapEditorPanel::UpdateCache()
{
	// Now, update the cache of the links.
	links.clear();

	for(const auto &it : GameData::Systems())
	{
		const System *system = &it.second;
		if(!system->IsValid())
			continue;

		for(const System *link : system->Links())
			if(link < system)
			{
				// Only draw links between two systems if both are
				// valid . Also, avoid drawing twice by only drawing in the
				// direction of increasing pointer values.
				if(!link->IsValid())
					continue;
				links.emplace_back(system->Position(), link->Position());
			}
	}
}



void MapEditorPanel::DrawWormholes()
{
	// Keep track of what arrows and links need to be drawn.
	set<pair<const System *, const System *>> arrowsToDraw;

	// Avoid iterating each StellarObject in every system by iterating over planets instead. A
	// system can host more than one set of wormholes (e.g. Cardea), and some wormholes may even
	// share a link vector. If a wormhole's planet has no description, no link will be drawn.
	for(auto &&it : GameData::Planets())
	{
		const Planet &p = it.second;
		if(!p.IsValid() || !p.IsWormhole() || p.Description().empty())
			continue;

		const vector<const System *> &waypoints = p.WormholeSystems();
		const System *from = waypoints.back();
		for(const System *to : waypoints)
		{
			arrowsToDraw.emplace(from, to);
			from = to;
		}
	}

	const Color &wormholeDim = *GameData::Colors().Get("map unused wormhole");
	const Color &arrowColor = *GameData::Colors().Get("map used wormhole");
	static const double ARROW_LENGTH = 4.;
	static const double ARROW_RATIO = .3;
	static const Angle LEFT(30.);
	static const Angle RIGHT(-30.);
	const double zoom = Zoom();

	for(const pair<const System *, const System *> &link : arrowsToDraw)
	{
		// Compute the start and end positions of the wormhole link.
		Point from = zoom * (link.first->Position() + center);
		Point to = zoom * (link.second->Position() + center);
		Point offset = (from - to).Unit() * MapPanel::LINK_OFFSET;
		from -= offset;
		to += offset;

		// If an arrow is being drawn, the link will always be drawn too. Draw
		// the link only for the first instance of it in this set.
		if(link.first < link.second || !arrowsToDraw.count(make_pair(link.second, link.first)))
			LineShader::Draw(from, to, MapPanel::LINK_WIDTH, wormholeDim);

		// Compute the start and end positions of the arrow edges.
		Point arrowStem = zoom * ARROW_LENGTH * offset;
		Point arrowLeft = arrowStem - ARROW_RATIO * LEFT.Rotate(arrowStem);
		Point arrowRight = arrowStem - ARROW_RATIO * RIGHT.Rotate(arrowStem);

		// Draw the arrowhead.
		Point fromTip = from - arrowStem;
		LineShader::Draw(from, fromTip, MapPanel::LINK_WIDTH, arrowColor);
		LineShader::Draw(from - arrowLeft, fromTip, MapPanel::LINK_WIDTH, arrowColor);
		LineShader::Draw(from - arrowRight, fromTip, MapPanel::LINK_WIDTH, arrowColor);
	}
}



void MapEditorPanel::DrawLinks()
{
	double zoom = Zoom();
	for(const auto &link : links)
	{
		Point from = zoom * (link.first + center);
		Point to = zoom * (link.second + center);
		Point unit = (from - to).Unit() * MapPanel::LINK_OFFSET;
		from -= unit;
		to += unit;

		LineShader::Draw(from, to, MapPanel::LINK_WIDTH, GameData::Colors().Get("map link")->Transparent(.5));
	}
}



void MapEditorPanel::DrawSystems()
{
	// Draw the circles for the systems.
	double zoom = Zoom();
	for(const auto &pair : GameData::Systems())
	{
		const auto &system = pair.second;
		if(!system.IsValid())
			continue;

		Point pos = zoom * (system.Position() + center);
		RingShader::Draw(pos, MapPanel::OUTER, MapPanel::INNER, GovernmentColor(system.GetGovernment()));
	}
}



void MapEditorPanel::DrawNames()
{
	// Don't draw if too small.
	double zoom = Zoom();
	if(zoom <= 0.5)
		return;

	// Draw names for all systems you have visited.
	bool useBigFont = (zoom > 2.);
	const Font &font = FontSet::Get(useBigFont ? 18 : 14);
	Point offset(useBigFont ? 8. : 6., -.5 * font.Height());
	for(const auto &pair : GameData::Systems())
		font.Draw(pair.second.Name(), zoom * (pair.second.Position() + center) + offset,
				GameData::Colors().Get("map name")->Transparent(.75));
}
