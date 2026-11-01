/* EscortWorkshopPanel.cpp
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

#include "EscortWorkshopPanel.h"

#include "Color.h"
#include "Command.h"
#include "DataFile.h"
#include "DataNode.h"
#include "shader/DrawList.h"
#include "shader/FillShader.h"
#include "Firmware.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "shader/LineShader.h"
#include "MouseButton.h"
#include "PlayerInfo.h"
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

using namespace std;

namespace {
	const double INSPECTOR_WIDTH = 270.;
	const double STATUS_HEIGHT = 36.;
	const double FOOTER_HEIGHT = 30.;
	// Start the drill over this often.
	const int LOOP_STEPS = 1800;
	// How long typing must pause before the program is compiled and run.
	const int QUIET_FRAMES = 20;
	// The distance within which "keep station" stops trying to adjust its
	// position (its "position-deadband").
	const double CLOSE = 200.;

	const Color BACKGROUND(.02f, .04f, .07f, 1.f);
	const Color GRID_MINOR(.04f, .09f, .15f, 1.f);
	const Color GRID_MAJOR(.08f, .17f, .28f, 1.f);
	const Color TEXT(.80f, .90f, 1.f, 1.f);
	const Color DIM(.45f, .58f, .72f, 1.f);
	const Color LEADER(.55f, .80f, 1.f, 1.f);
	const Color ESCORT(.98f, .75f, .30f, 1.f);
	const Color GOOD(.30f, .90f, .45f, 1.f);
	const Color BAD(.95f, .35f, .30f, 1.f);
	const Color PANEL(0.f, .02f, .04f, .85f);
	const Color GHOST(.98f * .3f, .75f * .3f, .30f * .3f, .3f);
	const Color RAIDER(.95f, .35f, .30f, 1.f);
	// The role to start from, for an escort without one.
	const string FIRST_ROLE = "Bodyguard";

	// Ships to try as the escort, if the player has none of their own.
	const vector<string> ESCORTS = {"Sparrow", "Hawk", "Clipper", "Argosy", "Bulk Freighter"};
}



EscortWorkshopPanel::EscortWorkshopPanel(PlayerInfo *player, const Ship &leader, const Ship &escort,
		const string &roleUuid)
	: player(player), leaderModel(&leader), simulation(make_unique<EscortSimulation>(leader, escort)),
	roleUuid(roleUuid), editor(document)
{
	SetIsFullScreen(true);
	SetInterruptible(false);
	string text = FactoryText();
	if(player && IsRole())
	{
		// The escort's own role, or else the one all escorts play.
		for(const string &uuid : {roleUuid, string("*")})
		{
			auto it = player->EscortRoles().find(uuid);
			if(it != player->EscortRoles().end())
			{
				text = it->second;
				break;
			}
		}
	}
	else if(player)
	{
		auto it = player->ManeuverPrograms().find("keep station");
		if(it != player->ManeuverPrograms().end())
			text = it->second;
	}
	document.SetText(text);
	Recompile();
	Layout();
}



void EscortWorkshopPanel::Step()
{
	justFocused = false;
	if(document.Version() != seenVersion)
	{
		seenVersion = document.Version();
		quietFrames = 0;
	}
	else if(quietFrames < QUIET_FRAMES)
		++quietFrames;
	if(document.Version() != compiledVersion && quietFrames >= QUIET_FRAMES)
		Recompile();
	if(statusTime)
		--statusTime;
	if(!paused && !isScrubbing)
	{
		// The arrow keys fly the leader (unless the editor has the keyboard).
		GuidanceDrill::Control control;
		if(!editor.HasFocus())
		{
			const Uint8 *keys = SDL_GetKeyboardState(nullptr);
			control.turn = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
			control.thrust = keys[SDL_SCANCODE_UP];
		}
		const bool flying = control.turn || control.thrust;
		if(flying || wasFlying)
			simulation->SetLeaderControl(control);
		wasFlying = flying;
		if(simulation->CurrentStep() >= LOOP_STEPS)
			simulation->Reset();
		const int destroyed = simulation->RaidersDestroyed();
		simulation->Step();
		// For tests: note each raider the escort destroys.
		static const bool log = getenv("ES_SIMULATOR_LOG");
		if(log && simulation->RaidersDestroyed() > destroyed)
		{
			printf("ESCORT WORKSHOP raider destroyed at step %d\n", simulation->CurrentStep());
			fflush(stdout);
		}
		// And each time the role changes its mind.
		static int lastBehavior = -1;
		const int behavior = simulation->RoleRan() && simulation->RoleOutput().chose
			? static_cast<int>(simulation->RoleOutput().behavior) : -1;
		if(log && IsRole() && behavior != lastBehavior)
		{
			static const char *const NAMES[] = {"keep station", "attack threat", "attack leader target",
				"attack target", "retreat", "independent"};
			printf("ESCORT WORKSHOP escort: %s at step %d\n", behavior < 0 ? "nothing" : NAMES[behavior],
				simulation->CurrentStep());
			fflush(stdout);
		}
		lastBehavior = behavior;
	}
	editor.SetRun(CodeEditor::RunNotes(IsRole() ? simulation->RoleTrace() : simulation->LastTrace()));
}



void EscortWorkshopPanel::Draw()
{
	Layout();
	const Point leader = simulation->Leader().Position();
	const Point escort = simulation->Escort().Position();
	// Keep every ship in view (the raider too, once it is close).
	Point low(min(leader.X(), escort.X()), min(leader.Y(), escort.Y()));
	Point high(max(leader.X(), escort.X()), max(leader.Y(), escort.Y()));
	if(simulation->HasRaider() && simulation->Raider().Position().Distance(leader) < 2500.)
	{
		const Point raider = simulation->Raider().Position();
		low = Point(min(low.X(), raider.X()), min(low.Y(), raider.Y()));
		high = Point(max(high.X(), raider.X()), max(high.Y(), raider.Y()));
	}
	const Point span = high - low + Point(1200., 1200.);
	const double fit = min(view.Width() / span.X(), view.Height() / span.Y());
	center += ((low + high) / 2. - center) * .1;
	zoom += (fit - zoom) * .1;

	FillShader::Fill(Point(), Screen::Dimensions(), BACKGROUND);
	DrawGrid();

	// Where the escort went before the last edit, and where both have been.
	for(size_t i = 1; i < ghost.size(); ++i)
		LineShader::Draw(ToScreen(ghost[i - 1]), ToScreen(ghost[i]), 1.f, GHOST, false);
	auto trail = [this](const deque<Point> &points, const Color &color) {
		for(size_t i = 1; i < points.size(); ++i)
		{
			const float alpha = .15f + .6f * static_cast<float>(i) / points.size();
			LineShader::Draw(ToScreen(points[i - 1]), ToScreen(points[i]), 1.5f,
				Color(color.Get()[0] * alpha, color.Get()[1] * alpha, color.Get()[2] * alpha, alpha), false);
		}
	};
	trail(simulation->LeaderTrail(), LEADER);
	trail(simulation->EscortTrail(), ESCORT);
	if(simulation->HasRaider())
		trail(simulation->RaiderTrail(), RAIDER);

	if(IsRole())
	{
		// Where the role sends the escort: its station, or the raider.
		const Point station = simulation->StationPoint();
		const Color &color = simulation->HasRaider() && station == simulation->Raider().Position() ? RAIDER : GOOD;
		LineShader::DrawDashed(ToScreen(escort), ToScreen(station), (station - escort).Unit(), 1.f, color, 6., 6.);
		const Point at = ToScreen(station);
		LineShader::Draw(at + Point(-8., 0.), at + Point(8., 0.), 1.5f, color, false);
		LineShader::Draw(at + Point(0., -8.), at + Point(0., 8.), 1.5f, color, false);
	}
	else
	{
		// How close counts as on station, and how far off the escort is.
		const double distance = simulation->Distance();
		const Color &state = distance <= CLOSE ? GOOD : BAD;
		RingShader::Draw(ToScreen(leader), CLOSE * zoom, CLOSE * zoom - 1.5,
			Color(state.Get()[0] * .5f, state.Get()[1] * .5f, state.Get()[2] * .5f, .5f));
		LineShader::DrawDashed(ToScreen(escort), ToScreen(leader), (leader - escort).Unit(), 1.f, state, 6., 6.);
	}

	// The ships, drawn at least big enough to see, each where it is.
	const double shipZoom = max(zoom, .3);
	vector<const Ship *> ships = {&simulation->Leader(), &simulation->Escort()};
	if(simulation->HasRaider())
		ships.push_back(&simulation->Raider());
	for(const Ship *ship : ships)
	{
		DrawList draw;
		draw.Clear(simulation->CurrentStep(), shipZoom);
		draw.SetCenter(ship->Position() - ToScreen(ship->Position()) / shipZoom);
		draw.AddUnblurred(*ship);
		draw.Draw();
	}
	const Font &font = FontSet::Get(14);
	font.Draw("you", ToScreen(leader) + Point(18., -20.), LEADER);
	font.Draw("escort", ToScreen(escort) + Point(18., -20.), ESCORT);
	if(simulation->HasRaider())
		font.Draw("raider", ToScreen(simulation->Raider().Position()) + Point(18., -20.), RAIDER);

	// The timeline.
	const double left = view.Left() + 20.;
	const double right = view.Right() - 20.;
	const double y = view.Bottom() - 18.;
	LineShader::Draw(Point(left, y), Point(right, y), 2.f, GRID_MAJOR, false);
	const double x = left + (right - left) * min(1., simulation->CurrentStep() / static_cast<double>(LOOP_STEPS - 1));
	LineShader::Draw(Point(left, y), Point(x, y), 2.f, DIM, false);
	FillShader::Fill(Point(x, y), Point(4., 12.), TEXT);

	editor.Draw();
	DrawInspector();
	DrawStatus();
}



bool EscortWorkshopPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	const bool control = mod & (KMOD_CTRL | KMOD_GUI);
	if(control && key == SDLK_s)
		Install();
	else if(control && key == SDLK_d)
	{
		document.SetText(FactoryText());
		if(player && IsRole())
			player->SetEscortRole(roleUuid, "");
		else if(player)
			player->SetManeuverProgram("keep station", "");
		status = IsRole() ? "Back to " + FIRST_ROLE + " (not installed: ctrl+s installs it)."
			: "Restored the factory's flight program for your escorts.";
		statusTime = 300;
	}
	else if(control && key == SDLK_SPACE)
		paused = !paused;
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
		paused = true;
		simulation->Step();
	}
	else if(key == SDLK_COMMA)
	{
		paused = true;
		simulation->ReplayTo(max(0, simulation->CurrentStep() - 1));
	}
	else if(key == SDLK_r)
	{
		simulation->ClearLeaderControls();
		simulation->Reset();
	}
	else if(key == SDLK_d)
	{
		const int count = simulation->Drills().size();
		simulation->SetDrill((simulation->DrillIndex() + ((mod & KMOD_SHIFT) ? count - 1 : 1)) % count);
		ghost.clear();
	}
	else if(key == SDLK_t)
		NextEscort();
	else if(key == SDLK_UP || key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_DOWN)
	{
		// Flying the leader: see Step().
	}
	else
		return false;
	return true;
}



bool EscortWorkshopPanel::TextInput(const string &text)
{
	if(justFocused)
	{
		justFocused = false;
		return true;
	}
	return editor.TextInput(text);
}



bool EscortWorkshopPanel::Click(int x, int y, MouseButton button, int clicks)
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
	if(fabs(y - (view.Bottom() - 18.)) <= 12. && x >= view.Left() + 12. && x <= view.Right() - 12.)
	{
		isScrubbing = true;
		paused = true;
		dragPoint = point;
		ScrubTo(x);
	}
	return true;
}



bool EscortWorkshopPanel::Drag(double dx, double dy)
{
	dragPoint += Point(dx, dy);
	if(isScrubbing)
	{
		ScrubTo(dragPoint.X());
		return true;
	}
	return isDragging && editor.Drag(dragPoint);
}



bool EscortWorkshopPanel::Release(int x, int y, MouseButton button)
{
	isScrubbing = false;
	isDragging = false;
	return true;
}



bool EscortWorkshopPanel::Scroll(double dx, double dy)
{
	if(editor.Area().Contains(UI::GetMouse()))
		return editor.Scroll(dy);
	return false;
}



void EscortWorkshopPanel::Recompile()
{
	compiledVersion = document.Version();
	map<size_t, string> errors;
	istringstream in(document.Text());
	DataFile file(in);
	const DataNode *node = nullptr;
	const string kind = IsRole() ? "role" : "maneuver";
	for(const DataNode &child : file)
		if(!node && child.Token(0) == kind && child.Size() >= 2)
			node = &child;
	if(!node)
	{
		errors[1] = IsRole() ? "The first line should be: role \"<name>\""
			: "The first line should be: maneuver \"keep station\"";
		editor.SetErrors(errors);
		hasErrors = true;
		return;
	}
	if(IsRole())
	{
		EscortRole candidate;
		candidate.Load(*node, false);
		for(const script::Program::Error &error : candidate.Program().Errors())
			errors.emplace(error.line, error.message);
		editor.SetErrors(errors);
		hasErrors = !errors.empty();
		if(hasErrors)
			return;
		if(role.IsValid())
			ghost = simulation->EscortTrail();
		const int step = simulation->CurrentStep();
		role = std::move(candidate);
		simulation->SetRole(&role);
		simulation->ReplayTo(step);
		return;
	}
	ShipManeuver candidate;
	candidate.Load(*node, false);
	for(const script::Program::Error &error : candidate.Program().Errors())
		errors.emplace(error.line, error.message);
	editor.SetErrors(errors);
	hasErrors = !errors.empty();
	if(hasErrors)
		return;

	// It compiles: replay the run so far with it, keeping the escort's course
	// before the edit to compare with.
	const bool isFirst = !program.IsValid();
	if(!isFirst)
		ghost = simulation->EscortTrail();
	program = std::move(candidate);
	simulation->SetProgram(&program);
	simulation->ReplayTo(simulation->CurrentStep());
}



void EscortWorkshopPanel::Install()
{
	if(document.Version() != compiledVersion)
		Recompile();
	if(hasErrors)
	{
		status = "Fix the mistakes before installing.";
		statusTime = 300;
		return;
	}
	if(!player)
	{
		status = "There is no pilot to install it for.";
		statusTime = 300;
		return;
	}
	if(IsRole())
	{
		// A role the game has is given as it is; an edited one is the player's,
		// and kept in their library.
		string text = document.Text();
		string name = Firmware::NameOf(text);
		const bool isGameScript = Firmware::IsGameScript("role", name);
		const bool unchanged = isGameScript && text == GameData::ScriptText("role", name);
		if(!unchanged && (isGameScript || name.empty()))
		{
			name = name.empty() ? "Escort role" : name + " (edited)";
			text = Firmware::Renamed(text, name);
			document.SetText(text);
			compiledVersion = document.Version();
		}
		if(!player->SetEscortRole(roleUuid, text))
			status = "That role doesn't compile.";
		else
		{
			if(!unchanged)
				player->SetLibraryProgram(name, text);
			status = (roleUuid == "*" ? "Installed: your escorts play " : "Installed: this escort plays ") + name
				+ (unchanged ? "." : ", saved in your library.");
		}
		statusTime = 300;
		return;
	}
	// As in the missile workshop: a program the game has is installed as it
	// is, and an edited one is the player's, and kept in their library.
	const bool isFactory = (document.Text() == FactoryText());
	string text = document.Text();
	string name = Firmware::NameOf(text);
	const bool isGameScript = Firmware::IsGameScript("maneuver", name);
	const bool unchanged = isGameScript && text == GameData::ScriptText("maneuver", name);
	if(!isFactory && !unchanged && (isGameScript || name.empty()))
	{
		name = "Escort program";
		text = Firmware::Renamed(text, name);
		document.SetText(text);
	}
	if(!player->SetManeuverProgram("keep station", isFactory ? string() : text))
		status = "That program doesn't compile.";
	else if(isFactory)
		status = "Your escorts fly the factory's program.";
	else if(unchanged)
		status = "Installed: your escorts fly " + name + ".";
	else
	{
		player->SetLibraryProgram(name, text);
		status = "Installed, and saved in your library as \"" + name + "\".";
	}
	statusTime = 300;
}



bool EscortWorkshopPanel::IsRole() const
{
	return !roleUuid.empty();
}



string EscortWorkshopPanel::FactoryText() const
{
	if(IsRole())
	{
		const string text = GameData::ScriptText("role", FIRST_ROLE);
		return text.empty() ? "role \"" + FIRST_ROLE + "\"\n\tkeep_station" : text;
	}
	string text = GameData::ScriptText("maneuver", "keep station");
	return text.empty() ? "maneuver \"keep station\"\n\tset forward true" : text;
}



void EscortWorkshopPanel::NextEscort()
{
	for(size_t tries = 0; tries < ESCORTS.size(); ++tries)
	{
		escortIndex = (escortIndex + 1) % ESCORTS.size();
		const Ship *model = GameData::Ships().Find(ESCORTS[escortIndex]);
		if(!model)
			continue;
		const int drill = simulation->DrillIndex();
		simulation = make_unique<EscortSimulation>(*leaderModel, *model);
		simulation->SetDrill(drill);
		simulation->SetProgram(program.IsValid() ? &program : nullptr);
		simulation->SetRole(IsRole() && role.IsValid() ? &role : nullptr);
		simulation->Reset();
		ghost.clear();
		status = "Your escort is a " + model->DisplayModelName() + ".";
		statusTime = 180;
		return;
	}
}



void EscortWorkshopPanel::Layout()
{
	const double editorWidth = min(640., Screen::Width() * .42);
	const double top = Screen::Top() + STATUS_HEIGHT + 6.;
	const double bottom = Screen::Bottom() - FOOTER_HEIGHT;
	editor.SetArea(Rectangle::WithCorners(Point(Screen::Left() + 8., top), Point(Screen::Left() + 8. + editorWidth,
		bottom)));
	view = Rectangle::WithCorners(Point(editor.Area().Right() + 8., top), Point(Screen::Right() - INSPECTOR_WIDTH,
		bottom));
}



Point EscortWorkshopPanel::ToScreen(const Point &position) const
{
	return (position - center) * zoom + view.Center();
}



void EscortWorkshopPanel::DrawGrid() const
{
	const Point topLeft = (Screen::TopLeft() - view.Center()) / zoom + center;
	const Point bottomRight = (Screen::BottomRight() - view.Center()) / zoom + center;
	const double spacing = zoom * 100. < 12. ? 500. : 100.;
	for(double x = floor(topLeft.X() / spacing) * spacing; x <= bottomRight.X(); x += spacing)
	{
		const bool major = fmod(fabs(x), 500.) < 1.;
		LineShader::Draw(ToScreen(Point(x, topLeft.Y())), ToScreen(Point(x, bottomRight.Y())), 1.f,
			major ? GRID_MAJOR : GRID_MINOR, false);
	}
	for(double y = floor(topLeft.Y() / spacing) * spacing; y <= bottomRight.Y(); y += spacing)
	{
		const bool major = fmod(fabs(y), 500.) < 1.;
		LineShader::Draw(ToScreen(Point(topLeft.X(), y)), ToScreen(Point(bottomRight.X(), y)), 1.f,
			major ? GRID_MAJOR : GRID_MINOR, false);
	}
}



void EscortWorkshopPanel::DrawInspector() const
{
	const Font &font = FontSet::Get(14);
	const double left = Screen::Right() - INSPECTOR_WIDTH;
	FillShader::Fill(Point(left + INSPECTOR_WIDTH / 2., 0.), Point(INSPECTOR_WIDTH, Screen::Height()), PANEL);
	double y = Screen::Top() + STATUS_HEIGHT + 14.;
	auto line = [&font, &y, left](const string &text, const Color &color) {
		font.Draw(text, Point(left + 12., y), color);
		y += 18.;
	};
	char text[96];
	line("Drill: " + simulation->Drills()[simulation->DrillIndex()].name, TEXT);
	line("(arrow keys: fly it yourself)", DIM);
	y += 10.;
	if(IsRole())
	{
		// The fight: how each ship is doing, and what the role decided.
		auto bar = [&](const string &label, double fraction, const Color &color) {
			font.Draw(label, Point(left + 12., y), DIM);
			const double x = left + 90.;
			const double width = INSPECTOR_WIDTH - 110.;
			FillShader::Fill(Point(x + width / 2., y + 7.), Point(width, 8.), GRID_MAJOR);
			FillShader::Fill(Point(x + width * fraction / 2., y + 7.), Point(width * fraction, 8.), color);
			y += 18.;
		};
		bar("you", simulation->LeaderHull(), LEADER);
		bar("escort", simulation->EscortHull(), ESCORT);
		if(simulation->HasRaider())
			bar("raider", simulation->RaiderHull(), RAIDER);
		else
			line("no raider (yet)", DIM);
		snprintf(text, sizeof(text), "raiders destroyed: %d", simulation->RaidersDestroyed());
		line(text, DIM);
		y += 10.;
		static const char *const WHAT[] = {"keeping station", "attacking the threat", "attacking your target",
			"attacking its target", "retreating", "left to the usual AI"};
		const EscortRole::Output &decided = simulation->RoleOutput();
		if(!simulation->RoleRan())
			line("the role failed this step", BAD);
		else if(!decided.chose)
			line("the role chose nothing (usual AI)", DIM);
		else
		{
			line(string("the escort is ") + WHAT[decided.behavior], TEXT);
			if(decided.behavior == EscortRole::KEEP_STATION)
			{
				snprintf(text, sizeof(text), "station: %.0f right, %.0f ahead", decided.station.X(),
					decided.station.Y());
				line(text, DIM);
			}
		}
		y += 10.;
		line("The escort is a " + simulation->Escort().DisplayModelName(), DIM);
		line("(t to try another)", DIM);
		return;
	}
	const double distance = simulation->Distance();
	snprintf(text, sizeof(text), "escort %.0f away", distance);
	line(text, distance <= CLOSE ? GOOD : BAD);
	snprintf(text, sizeof(text), "on average %.0f away", simulation->MeanDistance());
	line(text, DIM);
	snprintf(text, sizeof(text), "within %.0f %.0f%% of the time", CLOSE, 100. * simulation->FractionWithin(CLOSE));
	line(text, DIM);
	y += 10.;

	// What the program decided on the latest step.
	const ShipManeuver::Output &output = simulation->LastOutput();
	if(!simulation->LastRan())
		line("the program failed this step", BAD);
	else
	{
		snprintf(text, sizeof(text), "turn %+.2f (of 1)", output.turn);
		line(text, TEXT);
		line(output.back ? "thrust: reverse" : output.forward ? "thrust: forward" : "thrust: none", TEXT);
	}
	y += 10.;

	// Some of what it is given.
	const ShipManeuver::Input &input = simulation->LastInput();
	line("what it is given (some of it)", DIM);
	snprintf(text, sizeof(text), "speed %.2f, top speed %.2f", input.velocity.Length(), input.maxVelocity);
	line(text, DIM);
	snprintf(text, sizeof(text), "acceleration %.3f", input.acceleration);
	line(text, DIM);
	snprintf(text, sizeof(text), "turn-rate %.2f degrees", input.turnRate);
	line(text, DIM);
	snprintf(text, sizeof(text), "facing-degrees %.1f", input.facingDegrees);
	line(text, DIM);
	snprintf(text, sizeof(text), "target speed %.2f", input.targetVelocity.Length());
	line(text, DIM);
	snprintf(text, sizeof(text), "last-turn %+.2f", input.lastTurn);
	line(text, DIM);
	y += 10.;
	line("The escort is a " + simulation->Escort().DisplayModelName(), DIM);
	line("(t to try another)", DIM);
}



void EscortWorkshopPanel::DrawStatus() const
{
	const Font &font = FontSet::Get(14);
	FillShader::Fill(Point(0., Screen::Top() + STATUS_HEIGHT / 2.), Point(Screen::Width(), STATUS_HEIGHT), PANEL);
	const string what = IsRole() ? "role \"" + role.Name() + "\" for " + (roleUuid == "*" ? "all your escorts"
		: "this escort") : "maneuver \"keep station\"";
	const string line = "ESCORT WORKSHOP   " + what + "   step " + to_string(simulation->CurrentStep())
		+ (paused ? "  (paused)" : "");
	font.Draw(line, Point(Screen::Left() + 14., Screen::Top() + 11.), TEXT);
	const string &note = statusTime ? status : hasErrors
		? string("The program has mistakes, so the last version without any is running.") : string();
	font.Draw(note, Point(Screen::Right() - INSPECTOR_WIDTH - font.Width(note) - 14., Screen::Top() + 11.),
		statusTime ? ESCORT : BAD);

	FillShader::Fill(Point(0., Screen::Bottom() - FOOTER_HEIGHT / 2.), Point(Screen::Width(), FOOTER_HEIGHT), PANEL);
	const string help = editor.HasFocus()
		? "editing: esc stop editing   ctrl+s install   ctrl+d factory   ctrl+space pause   ctrl+z undo"
		: "e edit   arrows fly   space pause   , . step   d drill   t escort   r restart   ctrl+s install"
			"   ctrl+d factory   esc exit";
	font.Draw(help, Point(Screen::Left() + 14., Screen::Bottom() - FOOTER_HEIGHT + 8.), DIM);
}



void EscortWorkshopPanel::ScrubTo(double x)
{
	const double left = view.Left() + 20.;
	const double right = view.Right() - 20.;
	const double fraction = max(0., min(1., (x - left) / (right - left)));
	const int step = static_cast<int>(lround(fraction * (LOOP_STEPS - 1)));
	if(step != simulation->CurrentStep())
		simulation->ReplayTo(step);
}
