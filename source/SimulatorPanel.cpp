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

#include "Angle.h"
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
#include "PlayerInfo.h"
#include "shader/PointerShader.h"
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
	// The size of the close-up of the followed missile.
	const double CLOSE_UP_SIZE = 246.;
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
	// The missile whose values the editor shows.
	const Color FOLLOWED = SELECTED;
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



SimulatorPanel::SimulatorPanel(const Outfit &outfit, const Ship &target, PlayerInfo *player)
	: outfit(outfit), player(player), simulation(make_unique<GuidanceSimulation>(outfit, target)), editor(document)
{
	SetIsFullScreen(true);
	// Escape belongs to this panel (to stop editing, or leave), not the menu.
	SetInterruptible(false);
	// Start with the script this weapon really uses: the player's program, if
	// there is one, or the factory's.
	const Weapon &weapon = simulation->GetWeapon();
	const MissileGuidance *original = weapon.Guidance() ? weapon.Guidance() : MissileGuidance::Default(weapon);
	guidanceName = original ? original->Name() : "basic";
	sourcePath = GameData::ScriptSource("guidance", guidanceName);
	string text = FactoryText();
	if(player)
	{
		auto it = player->GuidancePrograms().find(&outfit);
		if(it != player->GuidancePrograms().end())
			text = it->second;
	}
	document.SetText(text);
	Recompile();
	Layout();
}



void SimulatorPanel::Step()
{
	justFocused = false;
	if(document.Version() != compiledVersion)
		Recompile();
	if(statusTime)
		--statusTime;
	if(playFrames)
		--playFrames;
	if(rewindFrames)
		StepRewind();
	else if(holdFrames)
	{
		// Hold at the step the change first matters, and then play on.
		if(!--holdFrames)
			playFrames = 40;
	}
	else if(!paused && !isScrubbing)
	{
		FlyLauncher();
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
	UpdateFollow();
	UpdateRunNotes();
}



void SimulatorPanel::UpdateFollow()
{
	// Follow one missile from when it is fired until it hits or expires, and
	// then the next one fired after that. With none chosen, start with the
	// newest.
	const GuidanceSimulation::Missile *followed = simulation->Followed();
	if(!followed)
		return;
	if(followed->id != simulation->Selected())
	{
		simulation->Select(followed->id);
		return;
	}
	if(followed->endStep < 0)
		return;
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
		if(missile.endStep < 0 && missile.launchStep >= followed->endStep)
		{
			simulation->Select(missile.id);
			return;
		}
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
	// Ease the camera toward where it should be: between the two ships, but
	// moved over far enough to keep the followed missile in view (which only
	// matters when zoomed in).
	Point desired = (launcher + target) / 2.;
	const GuidanceSimulation::Missile *followed = simulation->Followed();
	if(followed && followed->endStep < 0)
	{
		const Point offset = (followed->projectile.Position() - desired) * zoom;
		const double marginX = max(0., view.Width() / 2. - 80.);
		const double marginY = max(0., view.Height() / 2. - 80.);
		const Point shift(copysign(max(0., fabs(offset.X()) - marginX), offset.X()),
			copysign(max(0., fabs(offset.Y()) - marginY), offset.Y()));
		desired += shift / zoom;
	}
	center += (desired - center) * .1;
	zoom += (fit - zoom) * .1;

	FillShader::Fill(Point(), Screen::Dimensions(), BACKGROUND);
	DrawGrid();
	DrawJamming();
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
	DrawTapeDeck();
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
	else if(control && key == SDLK_d && player)
	{
		// Back to the factory's program, on the player's ships too.
		document.SetText(FactoryText());
		player->SetGuidanceProgram(&outfit, "");
		status = "Restored the factory's guidance.";
		statusTime = 300;
	}
	else if(control && key == SDLK_r)
	{
		simulation->ClearLauncherControls();
		Restart();
	}
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
	{
		editor.SetFocus(true);
		justFocused = true;
	}
	else if(key == SDLK_SPACE)
		paused = !paused;
	else if(key == SDLK_PERIOD)
	{
		FinishRewind();
		holdFrames = 0;
		paused = true;
		simulation->Step();
	}
	else if(key == SDLK_COMMA)
	{
		// Replaying is exact, so stepping back is replaying to one step less.
		FinishRewind();
		holdFrames = 0;
		paused = true;
		simulation->ReplayTo(max(0, simulation->CurrentStep() - 1));
	}
	else if(key == SDLK_r)
	{
		// Start over, with the launcher back where it started.
		simulation->ClearLauncherControls();
		Restart();
	}
	else if(key == SDLK_UP || key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_DOWN)
	{
		// Flying the launcher: see FlyLauncher().
	}
	else if(key == SDLK_d)
	{
		const int count = simulation->Drills().size();
		const int next = (simulation->Drill() + ((mod & KMOD_SHIFT) ? count - 1 : 1)) % count;
		Restart();
		simulation->SetDrill(next);
	}
	else if(key == SDLK_t)
		NextTarget();
	else if(key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET)
		CycleMissile(key == SDLK_RIGHTBRACKET ? 1 : -1);
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
	if(justFocused)
	{
		justFocused = false;
		return true;
	}
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
	// Clicking or dragging on the timeline goes to that step.
	if(fabs(y - TimelineY()) <= 12. && x >= TimelineLeft() - 8. && x <= TimelineRight() + 8.)
	{
		FinishRewind();
		holdFrames = 0;
		isScrubbing = true;
		paused = true;
		dragPoint = point;
		ScrubTo(x);
		return true;
	}
	// Clicking a missile follows it; clicking anywhere else goes back to
	// following the newest one.
	if(view.Contains(point))
		simulation->Select(simulation->MissileAt(ToWorld(point), 60. / zoom));
	return true;
}



bool SimulatorPanel::Drag(double dx, double dy)
{
	if(isScrubbing)
	{
		dragPoint += Point(dx, dy);
		ScrubTo(dragPoint.X());
		return true;
	}
	if(!isDragging)
		return false;
	dragPoint += Point(dx, dy);
	return editor.Drag(dragPoint);
}



bool SimulatorPanel::Release(int x, int y, MouseButton button)
{
	isScrubbing = false;
	isDragging = false;
	return true;
}



void SimulatorPanel::ScrubTo(double x)
{
	const double fraction = (x - TimelineLeft()) / (TimelineRight() - TimelineLeft());
	const int step = static_cast<int>(lround(max(0., min(1., fraction)) * (LOOP_STEPS - 1)));
	if(step != simulation->CurrentStep())
		simulation->ReplayTo(step);
}



double SimulatorPanel::TimelineLeft() const
{
	return view.Left() + 20.;
}



double SimulatorPanel::TimelineRight() const
{
	return view.Right() - 20.;
}



double SimulatorPanel::TimelineY() const
{
	return view.Bottom() - 18.;
}



bool SimulatorPanel::Scroll(double dx, double dy)
{
	if(editor.Area().Contains(UI::GetMouse()))
		return editor.Scroll(dy);
	zoomFactor = max(.25, min(4., zoomFactor * pow(1.2, dy)));
	return true;
}



string SimulatorPanel::Blame(const GuidanceSimulation::Missile &missile) const
{
	// Jamming comes from the target; otherwise the seeker just isn't good
	// enough to hold on.
	const string cause = missile.projectile.ConfusionCause();
	if(cause == "poor tracking")
		return "its own poor tracking";
	return "the " + simulation->Target().DisplayModelName() + "'s " + cause;
}



void SimulatorPanel::FlyLauncher()
{
	// The arrow keys fly the launcher, unless the editor has the keyboard.
	// While they are up, the launcher flies as it was recorded flying, if it
	// was, so going back and playing again repeats it.
	GuidanceDrill::Control control;
	if(!editor.HasFocus())
	{
		const Uint8 *keys = SDL_GetKeyboardState(nullptr);
		control.turn = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
		control.thrust = keys[SDL_SCANCODE_UP];
	}
	const bool flying = control.turn || control.thrust;
	if(flying || wasFlying)
		simulation->SetLauncherControl(control);
	wasFlying = flying;
}



void SimulatorPanel::CycleMissile(int direction)
{
	// Missiles in flight, oldest first.
	vector<int> ids;
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
		if(missile.endStep < 0)
			ids.push_back(missile.id);
	if(ids.empty())
		return;
	const GuidanceSimulation::Missile *followed = simulation->Followed();
	auto it = followed ? find(ids.begin(), ids.end(), followed->id) : ids.end();
	int index = (it == ids.end()) ? static_cast<int>(ids.size()) - 1 : static_cast<int>(it - ids.begin());
	index = (index + direction + static_cast<int>(ids.size())) % static_cast<int>(ids.size());
	simulation->Select(ids[index]);
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
	rewindFrames = static_cast<int>(round(10. + 20. * log10(now - difference.step + 1.)));
	status = "This first makes a difference at step " + to_string(difference.step) + ", for missile #"
		+ to_string(difference.missile + 1) + ".";
	statusTime = 300;
}



void SimulatorPanel::StepRewind()
{
	if(++rewindFrame >= rewindFrames)
	{
		FinishRewind();
		// Hold there for a moment, before the new run plays on.
		holdFrames = 45;
		return;
	}
	// Wind back quickly at first, then slow down coming up to the step.
	const double t = static_cast<double>(rewindFrame) / rewindFrames;
	const double eased = 1. - (1. - t) * (1. - t) * (1. - t);
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
	const GuidanceSimulation::Missile *followed = simulation->Followed();
	if(!followed || followed->id != simulation->Selected())
		simulation->Select(difference.missile);
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
	// The notes are always for one missile in flight: the one clicked on, or
	// the newest. The caption says which, in the color it is drawn in.
	map<size_t, CodeEditor::LineNote> notes;
	const GuidanceSimulation::Missile *missile = simulation->Followed();
	if(missile)
	{
		notes = CodeEditor::RunNotes(simulation->TraceOf(missile->id));
		const string state = missile->endStep < 0 ? "" : missile->hit ? " (its last step: it hit)"
			: " (its last step: it expired)";
		editor.SetCaption("values from missile #" + to_string(missile->id + 1) + state, FOLLOWED);
	}
	else
		editor.SetCaption("no missile in flight", DIM);
	editor.SetRun(notes);
	// The code stays readable while the seeker has taken over, since it can
	// still be edited.
	editor.SetGlitch(missile && missile->endStep < 0 && missile->projectile.IsConfused() ? "SEEKER CONFUSED by "
		+ Blame(*missile)
		+ ": it steers, not this code" : "", false);
}



void SimulatorPanel::Save()
{
	if(hasErrors)
	{
		status = "Fix the mistakes before saving.";
		statusTime = 300;
		return;
	}
	// In the workshop, saving installs the program on the player's ships,
	// named for the weapon, so that it can't be mistaken for any other.
	if(player)
	{
		const bool isFactory = (document.Text() == FactoryText());
		const string programName = outfit.DisplayName() + " program";
		string text = document.Text();
		if(!isFactory && text.compare(0, 9, "guidance ") == 0)
		{
			const size_t end = text.find('\n');
			const string firstLine = "guidance \"" + programName + "\"";
			if(text.substr(0, end) != firstLine)
			{
				text = firstLine + (end == string::npos ? string() : text.substr(end));
				document.SetText(text);
			}
		}
		if(player->SetGuidanceProgram(&outfit, isFactory ? string() : text))
			status = isFactory ? "Your " + outfit.DisplayName() + "s fly the factory's guidance."
				: "Installed: your " + outfit.DisplayName() + "s will fly this program.";
		else
			status = "This weapon's hardware can't run that program.";
		statusTime = 300;
		return;
	}
	// Otherwise, saving puts the script in a plugin, where it replaces the
	// script of the same name everywhere in the game.
	const filesystem::path folder = Files::Config() / "plugins" / "simulator scripts" / "data";
	Files::CreateFolder(Files::Config() / "plugins");
	Files::CreateFolder(Files::Config() / "plugins" / "simulator scripts");
	Files::CreateFolder(folder);
	const filesystem::path path = folder / ("guidance " + guidanceName + ".txt");
	Files::Write(path, document.Text() + "\n");
	status = "Saved to the \"simulator scripts\" plugin.";
	statusTime = 300;
}



string SimulatorPanel::FactoryText() const
{
	string text = GameData::ScriptText("guidance", guidanceName);
	return text.empty() ? "guidance \"" + guidanceName + "\"\n\tset turn 0" : text;
}



void SimulatorPanel::NextTarget()
{
	// Ships that fly differently, and one with radar jammers.
	static const vector<string> TARGETS = {"Argosy", "Carrier", "Headhunter", "Clipper", "Bastion"};
	for(size_t tries = 0; tries < TARGETS.size(); ++tries)
	{
		targetIndex = (targetIndex + 1) % TARGETS.size();
		const Ship *model = GameData::Ships().Find(TARGETS[targetIndex]);
		if(!model)
			continue;
		const int drill = simulation->Drill();
		simulation = make_unique<GuidanceSimulation>(outfit, *model);
		simulation->SetDrill(drill);
		Restart();
		status = "Firing at a " + model->DisplayModelName() + ".";
		statusTime = 180;
		return;
	}
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
		const bool isSelected = (&missile == simulation->Followed());
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



void SimulatorPanel::DrawTapeDeck() const
{
	// Rewinding: two triangles pointing back, a little shaky, with a few
	// faint bands of tracking noise rolling across the range.
	const Point corner = view.TopLeft() + Point(36., 34.);
	const Font &font = FontSet::Get(18);
	if(rewindFrames)
	{
		const int jitter = (frameCount * 7 % 5 == 0) ? (frameCount % 3) - 1 : 0;
		const Point at = corner + Point(jitter, 0.);
		PointerShader::Draw(at, Point(-1., 0.), 22.f, 18.f, 0.f, MATTERED);
		PointerShader::Draw(at + Point(16., 0.), Point(-1., 0.), 22.f, 18.f, 0.f, MATTERED);
		font.Draw("REWIND", at + Point(34., -10.), MATTERED);
		for(int i = 0; i < 3; ++i)
		{
			const double y = view.Top() + fmod(frameCount * 9. + i * 197., view.Height());
			FillShader::Fill(Point(view.Center().X(), y), Point(view.Width(), 2. + i),
				Color(MATTERED.Get()[0] * .08f, MATTERED.Get()[1] * .08f, MATTERED.Get()[2] * .08f, .08f));
		}
	}
	else if(holdFrames || playFrames)
	{
		// Then one triangle pointing ahead: steady while holding at the step
		// where the change first matters, fading once it plays on.
		const float alpha = holdFrames ? 1.f : playFrames / 40.f;
		const Color color(MATTERED.Get()[0] * alpha, MATTERED.Get()[1] * alpha, MATTERED.Get()[2] * alpha, alpha);
		PointerShader::Draw(corner, Point(1., 0.), 22.f, 18.f, 0.f, color);
		font.Draw(holdFrames ? "step " + to_string(simulation->CurrentStep()) : "PLAY", corner + Point(16., -10.),
			color);
	}
}



void SimulatorPanel::DrawJamming() const
{
	// The target's jamming, for the kinds of seeker this weapon has: strongest
	// at the target, and fading out evenly to nothing at its edge.
	const Weapon &weapon = simulation->GetWeapon();
	const Ship &target = simulation->Target();
	double jamming = 0.;
	string kind;
	if(weapon.RadarTracking() && target.RadarJamming())
	{
		jamming = target.RadarJamming();
		kind = "radar";
	}
	if(weapon.OpticalTracking() && target.OpticalJamming() > jamming)
	{
		jamming = target.OpticalJamming();
		kind = "optical";
	}
	if(!jamming)
		return;
	const Point center = ToScreen(target.Position());
	const double range = Projectile::JammingRange(jamming) * zoom;
	const int BANDS = 12;
	for(int i = 0; i < BANDS; ++i)
	{
		const double outer = range * (i + 1) / BANDS;
		const double inner = range * i / BANDS;
		const float alpha = .22f * (1.f - (i + .5f) / BANDS);
		RingShader::Draw(center, outer, inner, Color(CONFUSED.Get()[0] * alpha, CONFUSED.Get()[1] * alpha,
			CONFUSED.Get()[2] * alpha, alpha));
	}
	const Color edge(CONFUSED.Get()[0] * .5f, CONFUSED.Get()[1] * .5f, CONFUSED.Get()[2] * .5f, .5f);
	RingShader::Draw(center, range, range - 1.5, edge);
	char text[64];
	snprintf(text, sizeof(text), "%s jamming %g", kind.c_str(), jamming);
	const Font &font = FontSet::Get(14);
	font.Draw(text, center + Point(-font.Width(text) / 2., -range - 20.), edge);
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
	const double left = TimelineLeft();
	const double right = TimelineRight();
	const double y = TimelineY();
	auto x = [left, right](int step) {
		return left + (right - left) * min(1., step / static_cast<double>(LOOP_STEPS - 1));
	};
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
	// Number each missile in flight, so the editor's caption can say which
	// one its values are from, colored by what its seeker is doing: locked on,
	// lost, or confused.
	const Font &font = FontSet::Get(14);
	const GuidanceSimulation::Missile *followed = simulation->Followed();
	vector<int> locked;
	for(const GuidanceSimulation::Missile &missile : simulation->Missiles())
	{
		if(missile.endStep >= 0)
			continue;
		const Projectile &projectile = missile.projectile;
		if(projectile.HasLock())
			locked.push_back(missile.id);
		if(&missile == followed)
			continue;
		const Color &color = projectile.IsConfused() ? CONFUSED : projectile.HasLock() ? HIT : MISS;
		const Point at = ToScreen(projectile.Position()) + Point(10., -16.);
		font.Draw("#" + to_string(missile.id + 1), at, color);
	}
	// Lock-on brackets around the target, while any seeker sees it, saying
	// which ones.
	if(!locked.empty())
	{
		const Point center = ToScreen(simulation->Target().Position());
		const double r = max(18., simulation->Target().Radius() * zoom + 6.);
		const double arm = r * .35;
		for(int sx : {-1, 1})
			for(int sy : {-1, 1})
			{
				const Point corner = center + Point(sx * r, sy * r);
				LineShader::Draw(corner, corner - Point(sx * arm, 0.), 1.5f, HIT, false);
				LineShader::Draw(corner, corner - Point(0., sy * arm), 1.5f, HIT, false);
			}
		string label = "locked:";
		for(int id : locked)
			label += " #" + to_string(id + 1);
		font.Draw(label, center + Point(-r, r + 6.), HIT);
	}
	if(!followed)
		return;
	const Point position = ToScreen(followed->projectile.Position());
	const string label = "#" + to_string(followed->id + 1);

	// A vector in the editor that the mouse is over, drawn from the missile:
	// to scale if it is long enough to see (like an offset to the target),
	// or else stretched, saying by how much (like a direction, or a velocity).
	if(const CodeEditor::LineNote *note = editor.NoteAt(UI::GetMouse()))
		if(note->isVector && note->vector)
		{
			const double length = note->vector.Length() * zoom;
			const double stretch = length < 60. ? 60. / length : 1.;
			const Point tip = position + note->vector * zoom * stretch;
			LineShader::Draw(position, tip, 2.f, TEXT, false);
			PointerShader::Draw(tip, note->vector.Unit(), 10.f, 10.f, 0.f, TEXT);
			string text = note->text.substr(0, note->text.find(" = "));
			if(stretch > 1.)
				text += " (not to scale)";
			font.Draw(text, tip + note->vector.Unit() * 12. + Point(-font.Width(text) / 2., -7.), TEXT);
		}
	if(followed->endStep < 0 && followed->projectile.IsConfused())
	{
		// The seeker has lost the target and is steering on its own.
		const double wobble = (frameCount / 3 % 3) - 1.;
		RingShader::Draw(position + Point(wobble, 0.), 16.f, 14.f, CONFUSED);
		font.Draw(label + " CONFUSED", position + Point(20., -8.), CONFUSED);
		return;
	}
	RingShader::Draw(position, 16.f, 14.f, FOLLOWED);
	font.Draw(label, position + Point(20., -8.), FOLLOWED);
	// Where the seeker thinks the target is, which with a noisy seeker is
	// not quite where it really is.
	const MissileGuidance::Input &input = followed->projectile.LastGuidanceInput();
	if(input.hasLock && followed->endStep < 0)
	{
		const Point seen = ToScreen(input.targetPosition);
		LineShader::DrawDashed(position, seen, (seen - position).Unit(), 1.f, FOLLOWED, 6., 6.);
		LineShader::Draw(seen + Point(-8., 0.), seen + Point(8., 0.), 1.5f, FOLLOWED);
		LineShader::Draw(seen + Point(0., -8.), seen + Point(0., 8.), 1.5f, FOLLOWED);
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
	y += 10.;
	const GuidanceSimulation::Missile *missile = simulation->Followed();
	if(!missile)
	{
		line("No missile in flight.", DIM);
		return;
	}
	line("Missile #" + to_string(missile->id + 1) + ", fired at step " + to_string(missile->launchStep), FOLLOWED);
	if(missile->endStep >= 0)
		line(string(missile->hit ? "Hit" : "Expired") + " at step " + to_string(missile->endStep)
			+ "; next one soon", missile->hit ? HIT : MISS);
	else
		line("[ ] or click to follow another", DIM);
	y += 6.;
	DrawCloseUp(*missile, Rectangle(Point(left + INSPECTOR_WIDTH / 2., y + CLOSE_UP_SIZE / 2.),
		Point(CLOSE_UP_SIZE, CLOSE_UP_SIZE)));
	y += CLOSE_UP_SIZE + 12.;

	// Its state, as gauges and numbers.
	const Projectile &projectile = missile->projectile;
	const Weapon &weapon = simulation->GetWeapon();
	const MissileGuidance::Input &input = projectile.LastGuidanceInput();
	const bool confused = missile->endStep < 0 && projectile.IsConfused();
	if(confused)
		line((frameCount / 15) % 2 ? "SEEKER CONFUSED" : "", CONFUSED);
	else
		line(string("lock: ") + (input.hasLock ? "target in sight" : "no target"), input.hasLock ? HIT : MISS);
	if(confused)
	{
		line("by " + Blame(*missile) + ";", CONFUSED);
		line("it steers itself, and the code waits", CONFUSED);
	}
	else if(input.hasLock)
		line("target " + to_string(lround((input.targetPosition - input.position).Length())) + " away", DIM);
	// Its odds at its next lock check, which jamming near the target lowers.
	if(!confused)
	{
		char text[64];
		snprintf(text, sizeof(text), "%s %.0f%% at the next check", projectile.HasLock() ? "keeps lock:" : "relocks:",
			100. * projectile.LockChance());
		line(text, projectile.LockChance() < .8 ? CONFUSED : DIM);
	}
	const int total = max(1, input.age + input.lifetimeLeft);
	auto gauge = [&](const string &label, double fraction, const string &value, const Color &color) {
		font.Draw(label, Point(left + 12., y), DIM);
		const double barLeft = left + 70.;
		const double barWidth = INSPECTOR_WIDTH - 70. - 70.;
		FillShader::Fill(Point(barLeft + barWidth / 2., y + 8.), Point(barWidth, 8.), GRID_MINOR);
		fraction = max(0., min(1., fraction));
		FillShader::Fill(Point(barLeft + barWidth * fraction / 2., y + 8.), Point(barWidth * fraction, 8.), color);
		font.Draw(value, Point(left + INSPECTOR_WIDTH - 12. - font.Width(value), y), DIM);
		y += 20.;
	};
	gauge("life", static_cast<double>(input.lifetimeLeft) / total, to_string(input.lifetimeLeft) + " left",
		input.lifetimeLeft < 60 ? MISS : HIT);
	const double topSpeed = weapon.Drag() ? weapon.Acceleration() / weapon.Drag() : 1.;
	const double speed = projectile.Velocity().Length();
	gauge("speed", speed / max(1., topSpeed), to_string(lround(speed * 60.)) + "/s", MISSILE);
	if(const MissileGuidance::Record *record = simulation->LastRecord(missile->id))
	{
		const double maxTurn = max(.001, weapon.Turn());
		char text[32];
		snprintf(text, sizeof(text), "%+.1f deg", confused ? 0. : record->output.turn);
		gauge("turn", fabs(record->output.turn) / maxTurn, confused ? "seeker" : text, confused ? CONFUSED : TEXT);
		snprintf(text, sizeof(text), "%.0f%%", 100. * record->output.thrust / max(1e-9, weapon.Acceleration()));
		gauge("thrust", confused ? 1. : record->output.thrust / max(1e-9, weapon.Acceleration()),
			confused ? "seeker" : text, confused ? CONFUSED : TEXT);
	}
}



void SimulatorPanel::DrawCloseUp(const GuidanceSimulation::Missile &missile, const Rectangle &box) const
{
	// The missile itself, magnified, facing as it does, with where it is
	// headed and where its seeker sees the target.
	FillShader::Fill(box.Center(), box.Dimensions(), BACKGROUND);
	const Projectile &projectile = missile.projectile;
	const bool confused = missile.endStep < 0 && projectile.IsConfused();
	const Color &frame = confused ? CONFUSED : FOLLOWED;
	const double wobble = confused ? (frameCount / 3 % 3) - 1. : 0.;
	const Point corners[] = {box.TopLeft(), Point(box.Right(), box.Top()), box.BottomRight(),
		Point(box.Left(), box.Bottom())};
	for(int i = 0; i < 4; ++i)
		LineShader::Draw(corners[i] + Point(wobble, 0.), corners[(i + 1) % 4] + Point(wobble, 0.), 1.f, frame, false);

	// Scale the sprite up to fill a good part of the box.
	const double size = max(1., max(projectile.Width(), projectile.Height()));
	const double scale = min(6., box.Width() * .45 / size);
	DrawList draw;
	draw.Clear(simulation->CurrentStep(), scale);
	draw.SetCenter(projectile.Position() - (box.Center() + Point(wobble, 0.)) / scale);
	draw.AddUnblurred(projectile);
	draw.Draw();

	// The forces on it: thrust pushing it along the way it faces, drag pulling
	// back against its velocity (the two balance at its top speed), and how far
	// it is turning this step, out of the most it can.
	const Weapon &weapon = simulation->GetWeapon();
	const Point center = box.Center() + Point(wobble, 0.);
	const double reach = box.Width() * .45;
	const Point facing = projectile.Facing().Unit();
	const Point velocity = projectile.Velocity();
	const double acceleration = max(1e-9, weapon.Acceleration());
	const double topSpeed = weapon.Drag() ? acceleration / weapon.Drag() : max(1., velocity.Length());
	const MissileGuidance::Record *record = simulation->LastRecord(missile.id);
	// A confused seeker steers at full turn, and keeps on thrusting.
	const double thrust = confused || !record ? acceleration : record->output.thrust;
	const double turn = (confused || !record) ? 0. : record->output.turn;
	const Font &font = FontSet::Get(14);
	auto arrow = [&](const Point &from, const Point &vector, const Color &color, const string &label) {
		if(vector.Length() < 2.)
			return;
		const Point tip = from + vector;
		LineShader::Draw(from, tip, 2.f, color, false);
		PointerShader::Draw(tip, vector.Unit(), 8.f, 8.f, 0.f, color);
		const Point at = tip + vector.Unit() * 10. + Point(-font.Width(label) / 2., -7.);
		font.Draw(label, Point(max(box.Left() + 4., min(box.Right() - 4. - font.Width(label), at.X())),
			max(box.Top() + 4., min(box.Bottom() - 18., at.Y()))), color);
	};
	// The exhaust: a flame out of its tail, as long as its thrust is strong.
	const double tail = size * scale * .5;
	if(thrust > 0.)
	{
		const Point start = center - facing * tail;
		const double length = reach * .5 * thrust / acceleration;
		LineShader::Draw(start, start - facing * length, 5.f, Color(.9f, .55f, .15f, .7f), false);
		LineShader::Draw(start, start - facing * length * .6, 2.f, Color(1.f, .9f, .5f, .9f), false);
	}
	if(velocity)
	{
		const double speed = velocity.Length();
		arrow(center, velocity.Unit() * reach * .85 * min(1., speed / topSpeed), MISSILE, "velocity");
		// Drag, off to one side so that it doesn't hide the exhaust.
		const Point side = Point(-velocity.Unit().Y(), velocity.Unit().X()) * 14.;
		if(weapon.Drag())
			arrow(center + side, -velocity.Unit() * reach * .55 * min(1., weapon.Drag() * speed / acceleration), MISS,
				"drag");
	}
	// The turn: an arc from the way it faces, out to where the most it can
	// turn would take it, filled as far as it is turning.
	const double maxTurn = weapon.Turn();
	if(maxTurn > 0.)
	{
		const double arcRadius = reach * .7;
		const double base = projectile.Facing().Degrees();
		const double span = 50.;
		auto at = [&](double degrees) { return center + Angle(base + degrees).Unit() * arcRadius; };
		for(int sign : {-1, 1})
			for(int i = 0; i < 10; ++i)
				LineShader::Draw(at(sign * span * i / 10.), at(sign * span * (i + 1) / 10.), 1.f, GRID_MAJOR, false);
		const double used = confused ? 0. : max(-1., min(1., turn / maxTurn)) * span;
		const int segments = max(1, static_cast<int>(fabs(used) / 5.));
		for(int i = 0; i < segments && used; ++i)
			LineShader::Draw(at(used * i / segments), at(used * (i + 1) / segments), 3.f, TEXT, false);
	}
	const MissileGuidance::Input &input = projectile.LastGuidanceInput();
	if(input.hasLock && !confused)
	{
		const Point toward = (input.targetPosition - input.position).Unit();
		LineShader::DrawDashed(center, center + toward * reach, toward, 1.f, FOLLOWED, 5., 5.);
	}
	font.Draw("#" + to_string(missile.id + 1), box.TopLeft() + Point(6., 4.), frame);
	if(confused)
	{
		const string text = "CONFUSED";
		font.Draw(text, Point(box.Center().X() - font.Width(text) / 2. + wobble, box.Top() + 4.), CONFUSED);
	}
}



void SimulatorPanel::DrawStatus() const
{
	const Font &font = FontSet::Get(14);
	FillShader::Fill(Point(0., Screen::Top() + STATUS_HEIGHT / 2.), Point(Screen::Width(), STATUS_HEIGHT), PANEL);
	string line = string(player ? "WORKSHOP   " : "SIMULATOR   ") + outfit.DisplayName() + " vs. "
		+ simulation->Target().DisplayModelName()
		+ "   guidance \"" + guidanceName + "\"   step " + to_string(simulation->CurrentStep())
		+ (paused ? "  (paused)" : "  " + FormatSpeed(SPEEDS[speedIndex]));
	font.Draw(line, Point(Screen::Left() + 14., Screen::Top() + 11.), TEXT);
	const string &note = statusTime ? status : hasErrors ? NOT_RUNNING : string();
	font.Draw(note, Point(Screen::Right() - INSPECTOR_WIDTH - font.Width(note) - 14., Screen::Top() + 11.),
		statusTime ? SELECTED : MISS);

	FillShader::Fill(Point(0., Screen::Bottom() - FOOTER_HEIGHT / 2.), Point(Screen::Width(), FOOTER_HEIGHT), PANEL);
	const string save = player ? "ctrl+s install   ctrl+d factory" : "ctrl+s save";
	const string help = editor.HasFocus()
		? "editing: esc stop editing   " + save + "   ctrl+space pause   ctrl+r restart   ctrl+z undo"
		: "e edit   arrows fly   space pause   , . step   - + speed   d drill   t target   r restart   [ ] missile   "
			+ save + "   esc exit";
	font.Draw(help, Point(Screen::Left() + 14., Screen::Bottom() - FOOTER_HEIGHT + 8.), DIM);
}
