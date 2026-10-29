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
#include "DataFile.h"
#include "DataNode.h"
#include "shader/DrawList.h"
#include "Files.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "shader/LineShader.h"
#include "MouseButton.h"
#include "Outfit.h"
#include "shader/RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "UI.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {
	const double SPEEDS[] = {.25, .5, 1., 2., 4., 8.};
	const int SPEED_COUNT = 6;
	const double INSPECTOR_WIDTH = 270.;
	const double STATUS_HEIGHT = 36.;
	const double FOOTER_HEIGHT = 30.;
	// Start the drill over this often, so replaying after an edit is quick.
	const int LOOP_STEPS = 1200;

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

	const string NOT_RUNNING = "The script has mistakes, so the last version without any is running.";

	// The lines of a data file that define the given node: from its first
	// line up to the next line that isn't indented.
	string NodeText(const filesystem::path &path, const string &kind, const string &name)
	{
		if(path.empty())
			return "";
		size_t start = 0;
		for(const DataNode &node : DataFile(path))
			if(node.Token(0) == kind)
				for(int i = 1; i < node.Size(); ++i)
					if(node.Token(i) == name)
						start = node.LineNumber();
		if(!start)
			return "";
		ifstream in(path);
		vector<string> lines;
		for(string line; getline(in, line); )
			lines.push_back(line);
		string text;
		for(size_t i = start - 1; i < lines.size(); ++i)
		{
			const string &line = lines[i];
			if(i >= start && !line.empty() && line[0] != '\t' && line[0] != ' ')
				break;
			text += line + "\n";
		}
		// Leave off blank lines at the end.
		while(text.size() > 1 && text[text.size() - 2] == '\n')
			text.pop_back();
		if(!text.empty() && text.back() == '\n')
			text.pop_back();
		return text;
	}

	string FormatSpeed(double speed)
	{
		char text[16];
		snprintf(text, sizeof(text), "%gx", speed);
		return text;
	}
}



SimulatorPanel::SimulatorPanel(const Outfit &outfit, const Ship &target)
	: outfit(outfit), simulation(make_unique<GuidanceSimulation>(outfit, target)), editor(document)
{
	SetIsFullScreen(true);
	// Start with the script this weapon really uses.
	const Weapon &weapon = simulation->GetWeapon();
	const MissileGuidance *original = weapon.Guidance() ? weapon.Guidance() : MissileGuidance::Default(weapon);
	guidanceName = original ? original->Name() : "basic";
	sourcePath = GameData::ScriptSource("guidance", guidanceName);
	string text = NodeText(sourcePath, "guidance", guidanceName);
	if(text.empty())
		text = "guidance \"" + guidanceName + "\"\n\tset turn 0";
	document.SetText(text);
	Recompile();
	Layout();
}



void SimulatorPanel::Step()
{
	if(document.Version() != compiledVersion)
		Recompile();
	if(statusTime)
		--statusTime;
	if(!paused)
	{
		// Fractional speeds step every few frames.
		const double speed = SPEEDS[speedIndex];
		++frame;
		const int steps = speed < 1. ? (frame % static_cast<int>(round(1. / speed)) == 0) : static_cast<int>(speed);
		for(int i = 0; i < steps; ++i)
		{
			if(simulation->CurrentStep() >= LOOP_STEPS)
				simulation->Reset();
			simulation->Step();
		}
	}
	UpdateRunNotes();
}



void SimulatorPanel::Draw()
{
	Layout();
	// Frame the launcher and the target.
	const Point launcher = simulation->Launcher().Position();
	const Point target = simulation->Target().Position();
	const Point span = Point(fabs(target.X() - launcher.X()), fabs(target.Y() - launcher.Y())) + Point(900., 900.);
	const double fit = min(view.Width() / span.X(), view.Height() / span.Y()) * zoomFactor;
	// Ease the camera toward where it should be.
	center += ((launcher + target) / 2. - center) * .1;
	zoom += (fit - zoom) * .1;

	FillShader::Fill(Point(), Screen::Dimensions(), BACKGROUND);
	DrawGrid();
	DrawTrails();

	DrawList draw;
	draw.Clear(simulation->CurrentStep(), zoom);
	draw.SetCenter(ToWorld(Point()));
	draw.Add(simulation->Launcher());
	draw.Add(simulation->Target());
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
		if(missile.endStep < 0)
			draw.Add(missile.projectile);
	draw.Draw();

	DrawSelection();
	editor.Draw();
	DrawInspector();
	DrawStatus();
}



bool SimulatorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	const bool control = mod & (KMOD_CTRL | KMOD_GUI);
	// Some keys work whether or not the editor has the keyboard.
	if(control && key == SDLK_s)
		Save();
	else if(control && key == SDLK_SPACE)
		paused = !paused;
	else if(control && key == SDLK_r)
		simulation->Reset();
	else if(editor.HasFocus())
	{
		if(key == SDLK_ESCAPE)
			editor.SetFocus(false);
		else
			editor.KeyDown(key, mod);
	}
	else if(key == SDLK_ESCAPE)
		GetUI().Pop(this);
	else if(key == SDLK_e || key == SDLK_RETURN)
		editor.SetFocus(true);
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



bool SimulatorPanel::TextInput(const string &text)
{
	return editor.TextInput(text);
}



bool SimulatorPanel::Click(int x, int y, MouseButton button, int clicks)
{
	if(button != MouseButton::LEFT)
		return false;
	const Point point(x, y);
	isDragging = false;
	if(editor.Area().Contains(point))
	{
		editor.SetFocus(true);
		editor.Click(point, clicks);
		isDragging = true;
		dragPoint = point;
		return true;
	}
	editor.SetFocus(false);
	if(view.Contains(point))
	{
		simulation->Select(simulation->MissileAt(ToWorld(point), 30. / zoom));
		// Replaying is exact, so replay to now to trace the latest step, even
		// while paused.
		simulation->ReplayTo(simulation->CurrentStep());
	}
	return true;
}



bool SimulatorPanel::Drag(double dx, double dy)
{
	if(!isDragging)
		return false;
	dragPoint += Point(dx, dy);
	return editor.Drag(dragPoint);
}



bool SimulatorPanel::Scroll(double dx, double dy)
{
	if(editor.Area().Contains(UI::GetMouse()))
		return editor.Scroll(dy);
	zoomFactor = max(.25, min(4., zoomFactor * pow(1.2, dy)));
	return true;
}



void SimulatorPanel::Recompile()
{
	compiledVersion = document.Version();
	map<size_t, string> errors;
	istringstream in(document.Text());
	DataFile file(in);
	const DataNode *node = nullptr;
	for(const DataNode &child : file)
		if(!node && child.Token(0) == "guidance" && child.Size() >= 2)
			node = &child;
	if(!node)
	{
		errors[1] = "The first line should be: guidance \"" + guidanceName + "\"";
		editor.SetErrors(errors);
		hasErrors = true;
		return;
	}
	MissileGuidance candidate;
	candidate.Load(*node, node->Token(1), false);
	for(const script::Program::Error &error : candidate.Program().Errors())
		errors.emplace(error.line, error.message);
	if(candidate.IsValid())
	{
		const string missing = candidate.MissingCapability(simulation->GetWeapon());
		if(!missing.empty())
			errors[node->LineNumber()] = "This weapon can't use it: " + missing;
	}
	editor.SetErrors(errors);
	hasErrors = !errors.empty();
	if(hasErrors)
		return;

	// It compiles: replay from the start with it, back to where things were.
	guidance = std::move(candidate);
	simulation->SetGuidance(&guidance);
	simulation->ReplayTo(simulation->CurrentStep());
}



void SimulatorPanel::UpdateRunNotes()
{
	map<size_t, CodeEditor::LineNote> notes;
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(missile && missile->endStep < 0)
	{
		using Kind = script::Trace::Step::Kind;
		const script::Trace &trace = simulation->SelectedTrace();
		for(const script::Trace::Step &step : trace.steps)
		{
			string text;
			switch(step.kind)
			{
				case Kind::ASSIGN:
					text = step.name + " = " + step.value.ToString();
					break;
				case Kind::CONDITION:
					text = step.value.ToString();
					break;
				case Kind::EFFECT:
					text = step.name;
					break;
				case Kind::STOP:
					text = "stopped";
					break;
				case Kind::RETURN:
					text = "returned " + step.value.ToString();
					break;
				case Kind::LOOP:
					text = "#" + step.value.ToString();
					break;
			}
			notes[step.line] = {text, false};
		}
		if(!trace.error.empty())
			notes[trace.errorLine] = {"error: " + trace.error, true};
	}
	editor.SetRun(notes);
}



void SimulatorPanel::Save()
{
	// Saving puts the script in a plugin, where it replaces the script of
	// the same name everywhere in the game.
	if(hasErrors)
	{
		status = "Fix the mistakes before saving.";
		statusTime = 300;
		return;
	}
	const filesystem::path folder = Files::Config() / "plugins" / "simulator scripts" / "data";
	Files::CreateFolder(Files::Config() / "plugins");
	Files::CreateFolder(Files::Config() / "plugins" / "simulator scripts");
	Files::CreateFolder(folder);
	const filesystem::path path = folder / ("guidance " + guidanceName + ".txt");
	Files::Write(path, document.Text() + "\n");
	status = "Saved to the \"simulator scripts\" plugin.";
	statusTime = 300;
}



void SimulatorPanel::Layout()
{
	const double editorWidth = min(640., Screen::Width() * .42);
	const double top = Screen::Top() + STATUS_HEIGHT + 6.;
	const double bottom = Screen::Bottom() - FOOTER_HEIGHT;
	editor.SetArea(Rectangle::WithCorners(Point(Screen::Left() + 8., top), Point(Screen::Left() + 8. + editorWidth,
		bottom)));
	view = Rectangle::WithCorners(Point(editor.Area().Right() + 8., top), Point(Screen::Right() - INSPECTOR_WIDTH,
		bottom));
}



Point SimulatorPanel::ToScreen(const Point &position) const
{
	return (position - center) * zoom + view.Center();
}



Point SimulatorPanel::ToWorld(const Point &screen) const
{
	return (screen - view.Center()) / zoom + center;
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
	double y = Screen::Top() + STATUS_HEIGHT + 14.;
	auto line = [&font, &y, left](const string &text, const Color &color) {
		font.Draw(text, Point(left + 12., y), color);
		y += 18.;
	};
	line("Drill " + to_string(simulation->Drill() + 1) + ": " + simulation->Drills()[simulation->Drill()].name, TEXT);
	line("hits " + to_string(simulation->Hits()) + " of " + to_string(simulation->Launches()) + " fired", DIM);
	y += 12.;
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(!missile)
	{
		line("Click a missile to see, in the", DIM);
		line("script, how it ran this step.", DIM);
		return;
	}
	line("Missile #" + to_string(missile->id + 1) + ", fired at step " + to_string(missile->launchStep), SELECTED);
	if(missile->endStep >= 0)
	{
		line(string(missile->hit ? "Hit" : "Expired") + " at step " + to_string(missile->endStep) + ".", DIM);
		return;
	}
	const MissileGuidance::Input &input = missile->projectile.LastGuidanceInput();
	line(string("lock: ") + (input.hasLock ? "yes" : input.confused ? "confused" : "no"), DIM);
	line("age " + to_string(input.age) + ", " + to_string(input.lifetimeLeft) + " steps left", DIM);
	if(input.hasLock)
	{
		const Point offset = input.targetPosition - input.position;
		line("target " + to_string(lround(offset.Length())) + " away", DIM);
	}
}



void SimulatorPanel::DrawStatus() const
{
	const Font &font = FontSet::Get(14);
	FillShader::Fill(Point(0., Screen::Top() + STATUS_HEIGHT / 2.), Point(Screen::Width(), STATUS_HEIGHT), PANEL);
	string line = "SIMULATOR   " + outfit.DisplayName() + " vs. " + simulation->Target().DisplayModelName()
		+ "   guidance \"" + guidanceName + "\"   step " + to_string(simulation->CurrentStep())
		+ (paused ? "  (paused)" : "  " + FormatSpeed(SPEEDS[speedIndex]));
	font.Draw(line, Point(Screen::Left() + 14., Screen::Top() + 11.), TEXT);
	const string &note = statusTime ? status : hasErrors ? NOT_RUNNING : string();
	font.Draw(note, Point(Screen::Right() - INSPECTOR_WIDTH - font.Width(note) - 14., Screen::Top() + 11.),
		statusTime ? SELECTED : MISS);

	FillShader::Fill(Point(0., Screen::Bottom() - FOOTER_HEIGHT / 2.), Point(Screen::Width(), FOOTER_HEIGHT), PANEL);
	const string help = editor.HasFocus()
		? "editing: esc stop editing   ctrl+s save   ctrl+space pause   ctrl+r restart   ctrl+z undo"
		: "e edit   space pause   . step   - + speed   1-8 drill   r restart   click a missile   esc exit";
	font.Draw(help, Point(Screen::Left() + 14., Screen::Bottom() - FOOTER_HEIGHT + 8.), DIM);
}
