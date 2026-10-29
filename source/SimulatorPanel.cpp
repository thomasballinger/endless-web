/* SimulatorPanel.cpp
Copyright (c) 2026 by Thomas Ballinger

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "SimulatorPanel.h"

#include "Color.h"
#include "Command.h"
#include "shader/DrawList.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "shader/LineShader.h"
#include "MissileGuidance.h"
#include "MouseButton.h"
#include "Outfit.h"
#include "shader/RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "UI.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace std;

namespace {
	const double SPEEDS[] = {.25, .5, 1., 2., 4., 8.};
	const int SPEED_COUNT = 6;
	const int INSPECTOR_WIDTH = 360;

	// The simulator's look: a dark green grid, not the game's starry space.
	const Color BACKGROUND(.02f, .06f, .04f, 1.f);
	const Color GRID_MINOR(.04f, .16f, .08f, 1.f);
	const Color GRID_MAJOR(.08f, .30f, .14f, 1.f);
	const Color TEXT(.75f, .95f, .80f, 1.f);
	const Color DIM(.45f, .65f, .50f, 1.f);
	const Color MISSILE(.98f, .60f, .24f, 1.f);
	const Color HIT(.30f, .90f, .45f, 1.f);
	const Color MISS(.95f, .35f, .30f, 1.f);
	const Color SELECTED(1.f, .95f, .40f, 1.f);
	const Color PANEL(0.f, .03f, .015f, .85f);

	string Describe(const script::Trace::Step &step)
	{
		using Kind = script::Trace::Step::Kind;
		string text = "line " + to_string(step.line) + "  " + string(2 * step.depth, ' ');
		switch(step.kind)
		{
			case Kind::ASSIGN:
				return text + step.name + " = " + step.value.ToString();
			case Kind::CONDITION:
				return text + "if: " + step.value.ToString();
			case Kind::EFFECT:
				return text + step.name;
			case Kind::STOP:
				return text + "stop";
			case Kind::RETURN:
				return text + "return " + step.value.ToString();
			case Kind::LOOP:
				return text + "for: " + step.value.ToString();
		}
		return text;
	}
}



SimulatorPanel::SimulatorPanel(const Outfit &outfit, const Ship &target)
	: outfit(outfit), simulation(make_unique<GuidanceSimulation>(outfit, target))
{
	SetIsFullScreen(true);
}



void SimulatorPanel::Step()
{
	if(paused)
		return;
	// Fractional speeds step every few frames.
	const double speed = SPEEDS[speedIndex];
	++frame;
	if(speed < 1.)
	{
		if(frame % static_cast<int>(round(1. / speed)) == 0)
			simulation->Step();
	}
	else
		for(int i = 0; i < static_cast<int>(speed); ++i)
			simulation->Step();
}



void SimulatorPanel::Draw()
{
	// Frame the launcher and the target, leaving room for the inspector.
	const Point launcher = simulation->Launcher().Position();
	const Point target = simulation->Target().Position();
	const Point viewSize = Screen::Dimensions() - Point(INSPECTOR_WIDTH, 120.);
	const Point span = Point(fabs(target.X() - launcher.X()), fabs(target.Y() - launcher.Y())) + Point(900., 900.);
	const double fit = min(viewSize.X() / span.X(), viewSize.Y() / span.Y()) * zoomFactor;
	const Point wanted = (launcher + target) / 2.;
	// Ease the camera toward where it should be.
	center += (wanted - center) * .1;
	zoom += (fit - zoom) * .1;

	FillShader::Fill(Point(), Screen::Dimensions(), BACKGROUND);
	DrawGrid();
	DrawTrails();

	DrawList draw;
	draw.Clear(simulation->CurrentStep(), zoom);
	// The view is shifted left to leave room for the inspector.
	draw.SetCenter(ToWorld(Point()));
	draw.Add(simulation->Launcher());
	draw.Add(simulation->Target());
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
		if(missile.endStep < 0)
			draw.Add(missile.projectile);
	draw.Draw();

	DrawSelection();
	DrawInspector();
	DrawStatus();
}



bool SimulatorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(key == SDLK_ESCAPE)
		GetUI().Pop(this);
	else if(key == SDLK_SPACE)
		paused = !paused;
	else if(key == SDLK_PERIOD)
	{
		paused = true;
		simulation->Step();
	}
	else if(key == SDLK_r)
		simulation->Reset();
	else if(key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS)
		speedIndex = min(SPEED_COUNT - 1, speedIndex + 1);
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		speedIndex = max(0, speedIndex - 1);
	else if(key >= SDLK_1 && key <= SDLK_9)
		simulation->SetDrill(static_cast<int>(key - SDLK_1));
	else
		return false;
	return true;
}



bool SimulatorPanel::Click(int x, int y, MouseButton button, int clicks)
{
	if(button != MouseButton::LEFT)
		return false;
	const Point world = ToWorld(Point(x, y));
	simulation->Select(simulation->MissileAt(world, 30. / zoom));
	// Replaying is exact, so replay to now to trace the latest step, even
	// while paused.
	simulation->ReplayTo(simulation->CurrentStep());
	return true;
}



bool SimulatorPanel::Scroll(double dx, double dy)
{
	zoomFactor = max(.25, min(4., zoomFactor * pow(1.2, dy)));
	return true;
}



Point SimulatorPanel::ToScreen(const Point &position) const
{
	return (position - center) * zoom - Point(INSPECTOR_WIDTH / 2., -30.);
}



Point SimulatorPanel::ToWorld(const Point &screen) const
{
	return (screen + Point(INSPECTOR_WIDTH / 2., -30.)) / zoom + center;
}



void SimulatorPanel::DrawGrid() const
{
	const Point topLeft = ToWorld(Screen::TopLeft());
	const Point bottomRight = ToWorld(Screen::BottomRight());
	// Lines every 100 units, and brighter ones every 500, unless they would
	// be too close together.
	const double spacing = zoom * 100. < 12. ? 500. : 100.;
	for(double x = floor(topLeft.X() / spacing) * spacing; x <= bottomRight.X(); x += spacing)
	{
		const bool major = fmod(fabs(x), 500.) < 1.;
		LineShader::Draw(ToScreen(Point(x, topLeft.Y())), ToScreen(Point(x, bottomRight.Y())),
			major ? 1.5f : 1.f, major ? GRID_MAJOR : GRID_MINOR, false);
	}
	for(double y = floor(topLeft.Y() / spacing) * spacing; y <= bottomRight.Y(); y += spacing)
	{
		const bool major = fmod(fabs(y), 500.) < 1.;
		LineShader::Draw(ToScreen(Point(topLeft.X(), y)), ToScreen(Point(bottomRight.X(), y)),
			major ? 1.5f : 1.f, major ? GRID_MAJOR : GRID_MINOR, false);
	}
}



void SimulatorPanel::DrawTrails() const
{
	const int step = simulation->CurrentStep();
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
	{
		const auto &trail = missile.trail;
		if(trail.size() < 2)
			continue;
		const bool isSelected = (missile.id == simulation->Selected());
		Color base = isSelected ? SELECTED : missile.hit ? HIT : missile.endStep >= 0 ? MISS : MISSILE;
		// Fade out a trail once its projectile is gone.
		float fade = missile.endStep < 0 ? 1.f : max(0.f, 1.f - (step - missile.endStep) / 120.f);
		if(isSelected)
			fade = 1.f;
		for(size_t i = 1; i < trail.size(); ++i)
		{
			const float age = static_cast<float>(i) / trail.size();
			const float alpha = fade * (.15f + .85f * age);
			const Color color(base.Get()[0] * alpha, base.Get()[1] * alpha, base.Get()[2] * alpha, alpha);
			LineShader::Draw(ToScreen(trail[i - 1]), ToScreen(trail[i]), isSelected ? 2.5f : 1.5f, color, false);
		}
		if(missile.endStep >= 0 && fade > 0.f)
		{
			const Color color(base.Get()[0] * fade, base.Get()[1] * fade, base.Get()[2] * fade, fade);
			RingShader::Draw(ToScreen(trail.back()), 9.f, 7.f, color);
		}
	}
}



void SimulatorPanel::DrawSelection() const
{
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(!missile || missile->endStep >= 0)
		return;
	const Point position = ToScreen(missile->projectile.Position());
	RingShader::Draw(position, 16.f, 14.f, SELECTED);
	// Where the seeker thinks the target is, which with a noisy seeker is
	// not quite where it really is.
	const MissileGuidance::Input &input = missile->projectile.LastGuidanceInput();
	if(input.hasLock)
	{
		const Point seen = ToScreen(input.targetPosition);
		LineShader::DrawDashed(position, seen, (seen - position).Unit(), 1.f, SELECTED, 6., 6.);
		LineShader::Draw(seen + Point(-8., 0.), seen + Point(8., 0.), 1.5f, SELECTED);
		LineShader::Draw(seen + Point(0., -8.), seen + Point(0., 8.), 1.5f, SELECTED);
	}
}



void SimulatorPanel::DrawInspector() const
{
	const Font &font = FontSet::Get(14);
	const double left = Screen::Right() - INSPECTOR_WIDTH;
	FillShader::Fill(Point(left + INSPECTOR_WIDTH / 2., 0.), Point(INSPECTOR_WIDTH, Screen::Height()), PANEL);
	double y = Screen::Top() + 50.;
	auto line = [&font, &y, left](const string &text, const Color &color) {
		font.Draw(text, Point(left + 14., y), color);
		y += 18.;
	};
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(!missile)
	{
		line("Click a missile to see how its", DIM);
		line("guidance script runs.", DIM);
		return;
	}
	line("Missile #" + to_string(missile->id + 1) + ", fired at step " + to_string(missile->launchStep), TEXT);
	if(missile->endStep >= 0)
	{
		line(string(missile->hit ? "Hit" : "Expired") + " at step " + to_string(missile->endStep) + ".", DIM);
		return;
	}
	const MissileGuidance::Input &input = missile->projectile.LastGuidanceInput();
	line(string("lock: ") + (input.hasLock ? "yes" : input.confused ? "confused" : "no")
		+ "   age " + to_string(input.age) + "   left " + to_string(input.lifetimeLeft), DIM);
	y += 8.;
	const script::Trace &trace = simulation->SelectedTrace();
	const int maxLines = static_cast<int>((Screen::Bottom() - y - 20.) / 18.);
	int count = 0;
	for(const script::Trace::Step &step : trace.steps)
	{
		if(++count > maxLines)
		{
			line("...", DIM);
			break;
		}
		line(Describe(step), step.depth ? DIM : TEXT);
	}
	if(!trace.error.empty())
		line("line " + to_string(trace.errorLine) + "  error: " + trace.error, MISS);
}



void SimulatorPanel::DrawStatus() const
{
	const Font &font = FontSet::Get(14);
	FillShader::Fill(Point(0., Screen::Top() + 18.), Point(Screen::Width(), 36.), PANEL);
	const double speed = SPEEDS[speedIndex];
	char speedText[16];
	snprintf(speedText, sizeof(speedText), "%gx", speed);
	const auto &drill = simulation->Drills()[simulation->Drill()];
	string status = "SIMULATOR   " + outfit.DisplayName() + " vs. " + simulation->Target().DisplayModelName()
		+ "   drill " + to_string(simulation->Drill() + 1) + ": " + drill.name
		+ "   step " + to_string(simulation->CurrentStep()) + (paused ? "  (paused)" : "  " + string(speedText))
		+ "   hits " + to_string(simulation->Hits()) + " / " + to_string(simulation->Launches());
	font.Draw(status, Point(Screen::Left() + 14., Screen::Top() + 10.), TEXT);
	const string help = "space pause   . step   - + speed   1-8 drill   r restart   click a missile   esc exit";
	font.Draw(help, Point(Screen::Left() + 14., Screen::Bottom() - 22.), DIM);
}
