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
#include <cstdio>
#include <cstdlib>
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
	const Color MATTERED(.55f, .80f, 1.f, 1.f);
	const Color CONFUSED(.95f, .40f, .95f, 1.f);
	const Color PANEL(0.f, .03f, .015f, .85f);

	const string NOT_RUNNING = "The script has mistakes, so the last version without any is running.";
	// How long the old run's trails linger after the new run passes where it was.
	const int GHOST_LINGER = 120;

	// With ES_SIMULATOR_LOG set, say what each edit did, for tests.
	void LogEdit(const GuidanceSimulation::Difference &difference, int now)
	{
		static const bool log = getenv("ES_SIMULATOR_LOG");
		if(!log)
			return;
		if(difference.step < 0)
			printf("SIMULATOR edit at step %d: no difference\n", now);
		else
			printf("SIMULATOR edit at step %d: first difference at step %d, missile %d\n", now, difference.step,
				difference.missile + 1);
		fflush(stdout);
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
	string text = GameData::ScriptText("guidance", guidanceName);
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
	if(rewindFrames)
		StepRewind();
	else if(!paused)
	{
		// Fractional speeds step every few frames.
		const double speed = SPEEDS[speedIndex];
		++frame;
		const int steps = speed < 1. ? (frame % static_cast<int>(round(1. / speed)) == 0) : static_cast<int>(speed);
		for(int i = 0; i < steps; ++i)
		{
			if(simulation->CurrentStep() >= LOOP_STEPS)
				Restart();
			simulation->Step();
		}
	}
	UpdateRunNotes();
}



void SimulatorPanel::Draw()
{
	++frameCount;
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
	DrawGhosts();
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
	DrawTimeline();
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
		Restart();
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
		FinishRewind();
		paused = true;
		simulation->Step();
	}
	else if(key == SDLK_r)
		Restart();
	else if(key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS)
		speedIndex = min(SPEED_COUNT - 1, speedIndex + 1);
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		speedIndex = max(0, speedIndex - 1);
	else if(key >= SDLK_1 && key <= SDLK_9)
	{
		Restart();
		simulation->SetDrill(static_cast<int>(key - SDLK_1));
	}
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
		FinishRewind();
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

	// It compiles. Any rewind under way skips to its end, where the script in
	// use takes over.
	FinishRewind();
	const int now = simulation->CurrentStep();
	const bool isFirst = !guidance.IsValid();
	difference = simulation->FirstDifference(candidate);
	if(!isFirst)
		LogEdit(difference, now);
	previousGuidance = std::move(guidance);
	guidance = std::move(candidate);
	ghosts.clear();
	ghostStep = -1;
	if(difference.step < 0)
	{
		// Nothing would have been different. Replaying still matters, to give
		// projectiles in flight what the new script would have remembered.
		simulation->SetGuidance(&guidance);
		simulation->ReplayTo(now);
		if(!isFirst)
		{
			status = "No difference yet: so far, this steers exactly as the last version did.";
			statusTime = 240;
		}
		return;
	}

	// Keep the old run's trails to compare with, and rewind through it to the
	// step where the new script first steers differently.
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
		ghosts.push_back(missile.trail);
	ghostStep = now;
	simulation->SetGuidance(&previousGuidance);
	rewindFrom = now;
	rewindTo = difference.step;
	rewindFrame = 0;
	rewindFrames = static_cast<int>(round(6. + 14. * log10(now - difference.step + 1.)));
	status = "This first makes a difference at step " + to_string(difference.step) + ", for missile #"
		+ to_string(difference.missile + 1) + ".";
	statusTime = 300;
}



void SimulatorPanel::StepRewind()
{
	if(++rewindFrame >= rewindFrames)
	{
		FinishRewind();
		return;
	}
	// Speed up and then slow down, like winding back a tape.
	const double t = static_cast<double>(rewindFrame) / rewindFrames;
	const double eased = t * t * (3. - 2. * t);
	simulation->ReplayTo(rewindFrom - static_cast<int>(lround((rewindFrom - rewindTo) * eased)));
}



void SimulatorPanel::FinishRewind()
{
	if(!rewindFrames)
		return;
	rewindFrames = 0;
	simulation->SetGuidance(&guidance);
	simulation->ReplayTo(rewindTo);
	// Show how the script ran for the projectile it first made a difference
	// for, unless another one in flight is selected.
	const GuidanceSimulation::Missile *selected = simulation->SelectedMissile();
	if(!selected || selected->endStep >= 0)
	{
		simulation->Select(difference.missile);
		simulation->ReplayTo(rewindTo);
	}
}



void SimulatorPanel::Restart()
{
	rewindFrames = 0;
	difference = GuidanceSimulation::Difference();
	ghosts.clear();
	ghostStep = -1;
	simulation->SetGuidance(&guidance);
	simulation->Reset();
}



void SimulatorPanel::UpdateRunNotes()
{
	map<size_t, CodeEditor::LineNote> notes;
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(missile && missile->endStep < 0)
		notes = CodeEditor::RunNotes(simulation->SelectedTrace());
	editor.SetRun(notes);
	editor.SetGlitch(missile && missile->endStep < 0 && missile->projectile.IsConfused()
		? "SEEKER CONFUSED: it is steering, not the script" : "");
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
			// Where the seeker was confused and steering, the trail shows it.
			const Color &segment = missile.confused[i] ? CONFUSED : base;
			const Color color(segment.Get()[0] * alpha, segment.Get()[1] * alpha, segment.Get()[2] * alpha, alpha);
			LineShader::Draw(ToScreen(trail[i - 1]), ToScreen(trail[i]), isSelected ? 2.5f : 1.5f, color, false);
		}
		if(missile.endStep >= 0 && fade > 0.f)
		{
			const Color color(base.Get()[0] * fade, base.Get()[1] * fade, base.Get()[2] * fade, fade);
			RingShader::Draw(ToScreen(trail.back()), 9.f, 7.f, color);
		}
	}
}



void SimulatorPanel::DrawGhosts() const
{
	// The old run's trails, until the new run is well past where it was.
	const int step = simulation->CurrentStep();
	if(rewindFrames || ghosts.empty() || step > ghostStep + GHOST_LINGER)
		return;
	const float fade = .35f * min(1.f, (ghostStep + GHOST_LINGER - step) / static_cast<float>(GHOST_LINGER));
	const Color color(MISSILE.Get()[0] * fade, MISSILE.Get()[1] * fade, MISSILE.Get()[2] * fade, fade);
	for(const deque<Point> &trail : ghosts)
		for(size_t i = 1; i < trail.size(); ++i)
			LineShader::Draw(ToScreen(trail[i - 1]), ToScreen(trail[i]), 1.f, color, false);

	// Where the new script first steered differently.
	if(difference.step >= 0)
	{
		const Point at = ToScreen(difference.position);
		RingShader::Draw(at, 7.f, 5.5f, MATTERED);
		FontSet::Get(14).Draw("step " + to_string(difference.step), at + Point(10., -18.), MATTERED);
	}
}



void SimulatorPanel::DrawTimeline() const
{
	// A bar under the range, from the start of the drill to where it loops.
	const double left = view.Left() + 20.;
	const double right = view.Right() - 20.;
	const double y = view.Bottom() - 18.;
	auto x = [left, right](int step) { return left + (right - left) * min(1., step / static_cast<double>(LOOP_STEPS)); };
	LineShader::Draw(Point(left, y), Point(right, y), 2.f, GRID_MAJOR, false);
	const int step = simulation->CurrentStep();
	LineShader::Draw(Point(left, y), Point(x(step), y), 2.f, rewindFrames ? MATTERED : DIM, false);
	if(ghostStep >= 0)
		LineShader::Draw(Point(x(ghostStep), y - 4.), Point(x(ghostStep), y + 4.), 1.f, DIM, false);
	if(difference.step >= 0)
	{
		const double at = x(difference.step);
		LineShader::Draw(Point(at, y - 7.), Point(at, y + 7.), 2.f, MATTERED, false);
		const Font &font = FontSet::Get(14);
		const string label = "made a difference at step " + to_string(difference.step);
		font.Draw(label, Point(min(at - 4., right - font.Width(label)), y - 26.), MATTERED);
	}
	FillShader::Fill(Point(x(step), y), Point(4., 12.), TEXT);
}



void SimulatorPanel::DrawSelection() const
{
	const GuidanceSimulation::Missile *missile = simulation->SelectedMissile();
	if(!missile || missile->endStep >= 0)
		return;
	const Point position = ToScreen(missile->projectile.Position());
	if(missile->projectile.IsConfused())
	{
		// The seeker has lost the target and is steering on its own.
		const double wobble = (frameCount / 3 % 3) - 1.;
		RingShader::Draw(position + Point(wobble, 0.), 16.f, 14.f, CONFUSED);
		FontSet::Get(14).Draw("CONFUSED", position + Point(20., -8.), CONFUSED);
		return;
	}
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
	if(missile->projectile.IsConfused())
	{
		line("lock: no, and confused: the seeker", MISS);
		line("is steering, not the script", MISS);
		return;
	}
	const MissileGuidance::Input &input = missile->projectile.LastGuidanceInput();
	line(string("lock: ") + (input.hasLock ? "yes" : "no"), DIM);
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
