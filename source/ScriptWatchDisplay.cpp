/* ScriptWatchDisplay.cpp
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

#include "ScriptWatchDisplay.h"

#include "CodeStyle.h"
#include "Color.h"
#include "EscortRole.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "shader/LineShader.h"
#include "MissileGuidance.h"
#include "Point.h"
#include "Screen.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {
	// Which kinds of script say the most about what a ship is doing, best
	// first. The player flies the flagship, so for it, its missiles come first.
	const vector<string> SHIP_KINDS = {"maneuver", "guidance", "choose", "rate", "turret"};
	const vector<string> FLAGSHIP_KINDS = {"guidance", "maneuver", "choose", "rate", "turret"};
	// The code in the pane changes this often, in frames, so it can be read.
	const int UPDATE_FRAMES = 6;
	// The pane sits beside the HUD's radar and target display.
	const double PANE_LEFT = 270.;
	const double PANE_TOP = 70.;
	const double PANE_BOTTOM = 170.;
	const double HEADER_HEIGHT = 92.;
	const double ROW_HEIGHT = 20.;

	const Color LABEL(.40f, .55f, .45f, 1.f);
	const Color DARK_LINE(.16f, .24f, .19f, .8f);
	const Color LIT_LINE(.35f, .90f, .50f, 1.f);
	const Color GLITCH(.95f, .40f, .95f, 1.f);
	const Color PANE(.01f, .035f, .02f, .92f);
	const Color TITLE(.75f, .95f, .80f, 1.f);
	const Color SELECTED(.55f, .80f, 1.f, 1.f);
	const Color BUTTON(.08f, .16f, .22f, 1.f);
	const Color HACK(.95f, .45f, .30f, 1.f);
	const Color HACK_BUTTON(.30f, .08f, .05f, 1.f);

	// A number that looks random but depends only on its arguments, so that
	// drawing never takes numbers from the game's random sequences.
	uint32_t Hash(uint32_t a, uint32_t b)
	{
		uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
		h = h ^ (h >> 15);
		h *= 0x2c1b3c6du;
		return h ^ (h >> 13);
	}

	// Shorten text to fit the given width.
	string Fit(const Font &font, string text, double width)
	{
		if(font.Width(text) <= width)
			return text;
		while(!text.empty() && font.Width(text + "...") > width)
			text.pop_back();
		return text + "...";
	}

	// Why an entry's code breaks up on screen, if it does.
	string Interference(const ScriptWatchDisplay::Entry &entry)
	{
		return !entry.activity.overriddenBy.empty() ? entry.activity.overriddenBy : entry.interference;
	}
}



ScriptWatchDisplay::ScriptWatchDisplay()
	: editor(document)
{
}



void ScriptWatchDisplay::Update(const vector<Entry> &entries, const Injector &injector)
{
	this->entries = entries;
	this->injector = injector;
	for(const Entry &entry : this->entries)
	{
		// Forget an entry's last run once it is something else.
		if(lastNames[entry.key] != entry.name)
		{
			lastNames[entry.key] = entry.name;
			lastRuns.erase(entry.key);
		}
		if(const script::Activity::Run *run = MainRun(entry))
			lastRuns[entry.key] = *run;
	}
	if(shown.empty())
		return;
	if(!Find(shown))
	{
		// Without a target (or that escort), show the flagship instead.
		if(Find("flagship"))
			Show("flagship");
		else
			shown.clear();
		return;
	}

	// Show what matters most, until the player picks something else.
	if(selectedName.empty())
		if(const script::Activity::Run *run = MainRun(*Find(shown)))
		{
			selectedKind = run->kind;
			selectedName = run->name;
		}
	if(++updates % UPDATE_FRAMES == 0 || loadedKey != selectedKind + " " + selectedName)
		ShowSelected();
	Layout();
}



void ScriptWatchDisplay::Draw(const Rectangle &flagshipBox, const Rectangle &targetBox) const
{
	++frame;
	const Entry *flagship = Find("flagship");
	const Entry *target = Find("target");
	flagshipArea = flagship ? flagshipBox : Rectangle();
	targetArea = target ? targetBox : Rectangle();
	if(flagship)
		DrawMap(flagshipBox, flagship);
	if(target)
		DrawMap(targetBox, target);
	if(!shown.empty())
		DrawPane();
}



void ScriptWatchDisplay::Toggle()
{
	if(!shown.empty())
		shown.clear();
	else if(Find("target"))
		Show("target");
	else if(Find("flagship"))
		Show("flagship");
}



bool ScriptWatchDisplay::IsEditing() const
{
	return editing && !shown.empty();
}



bool ScriptWatchDisplay::EditKey(SDL_Keycode key, Uint16 mod)
{
	if(key == SDLK_ESCAPE)
	{
		// Hand over the last edit, and fly on.
		if(document.Version() != submittedVersion)
		{
			pendingEdit = document.Text();
			submittedVersion = document.Version();
		}
		editing = false;
		editor.SetFocus(false);
		editNote.clear();
		return true;
	}
	editor.KeyDown(key, mod);
	return true;
}



bool ScriptWatchDisplay::EditText(const string &text)
{
	editor.TextInput(text);
	return true;
}



bool ScriptWatchDisplay::TakeEdit(string &text, string &uuid)
{
	if(pendingEdit.empty())
		return false;
	text = std::move(pendingEdit);
	pendingEdit.clear();
	uuid = editUuid;
	return true;
}



void ScriptWatchDisplay::SetEditResult(const map<size_t, string> &errors, const string &note)
{
	editor.SetErrors(errors);
	editNote = note;
	resultFrames = 0;
}



bool ScriptWatchDisplay::TakeCopy(string &text, string &from)
{
	if(copiedText.empty())
		return false;
	text = std::move(copiedText);
	from = std::move(copiedFrom);
	copiedText.clear();
	copiedFrom.clear();
	return true;
}



bool ScriptWatchDisplay::Click(const Point &point, string &hack)
{
	if(!shown.empty() && pane.Contains(point))
	{
		if(closeBox.Contains(point))
			shown.clear();
		else if(editBox.Contains(point) && !editing && Find(shown) && !Find(shown)->uuid.empty()
				&& selectedKind == "role")
		{
			// Edit this escort's role, with the game paused.
			editing = true;
			editUuid = Find(shown)->uuid;
			seenVersion = submittedVersion = document.Version();
			quietFrames = 0;
			editNote = "EDITING: the game is paused; esc to fly on";
			editor.SetFocus(true);
			return true;
		}
		else if(editing && editor.Area().Contains(point))
			return editor.Click(point, 1);
		else if(hackBox.Contains(point) && injector.isReady)
			Arm();
		else if(copyBox.Contains(point))
		{
			// Copy someone else's code, if it can be read clearly, into the
			// player's library.
			const Entry *entry = Find(shown);
			const bool copyable = (selectedKind == "guidance" || selectedKind == "maneuver");
			if(entry && !entry->isOwn && entry->unreadable.empty() && copyable)
			{
				if(!entry->interference.empty())
					injectResult = "COPY FAILED: jamming garbles the code";
				else
				{
					copiedText = GetSource(selectedKind, selectedName).text;
					copiedFrom = entry->name;
					injectResult = copiedText.empty() ? "COPY FAILED: the code isn't available"
						: "COPIED to your library";
					if(copiedText.empty())
						copiedFrom.clear();
				}
				resultFrames = 120;
			}
			return true;
		}
		else if(armedFrames && editor.Area().Contains(point))
		{
			// Inject into the line clicked: only one that ran is reachable.
			const CodeEditor::LineNote *note = editor.NoteAt(point);
			const bool ran = note && !note->isError;
			hack = (ran ? "" : "!") + shown;
			armedFrames = 0;
			injectResult = ran ? "INJECTED: that line ran, and the scripts crash" : "MISSED: that line didn't run";
			resultFrames = 120;
			return true;
		}
		for(const auto &[box, key] : entryButtons)
			if(box.Contains(point) && key != shown)
			{
				Show(key);
				return true;
			}
		const Entry *entry = Find(shown);
		for(const auto &[box, index] : tabs)
			if(entry && box.Contains(point) && index < entry->activity.runs.size())
			{
				selectedKind = entry->activity.runs[index].kind;
				selectedName = entry->activity.runs[index].name;
				ShowSelected();
				Layout();
			}
		return true;
	}

	string clicked;
	if(flagshipArea.Contains(point))
		clicked = "flagship";
	else if(targetArea.Contains(point))
		clicked = "target";
	else
		return false;

	// Clicking the code of what is shown closes it again.
	if(clicked == shown)
		shown.clear();
	else
		Show(clicked);
	return true;
}



bool ScriptWatchDisplay::Arm()
{
	const Entry *entry = Find(shown);
	if(!entry || entry->isOwn || !injector.isReady)
		return false;
	armedFrames = 300;
	resultFrames = 0;
	return true;
}



void ScriptWatchDisplay::Step()
{
	if(armedFrames && !--armedFrames)
	{
		injectResult = "The injector disarmed: no line was chosen.";
		resultFrames = 120;
	}
	if(resultFrames)
		--resultFrames;
	// Once the player stops typing for a moment, hand the edit over.
	if(editing)
	{
		if(document.Version() != seenVersion)
		{
			seenVersion = document.Version();
			quietFrames = 0;
		}
		else if(++quietFrames == 20 && document.Version() != submittedVersion)
		{
			pendingEdit = document.Text();
			submittedVersion = document.Version();
		}
	}
}



bool ScriptWatchDisplay::Scroll(const Point &point, double dy)
{
	if(shown.empty() || !pane.Contains(point))
		return false;
	editor.Scroll(dy);
	return true;
}



const string &ScriptWatchDisplay::Shown() const
{
	return shown;
}



const ScriptWatchDisplay::Source &ScriptWatchDisplay::GetSource(const string &kind, const string &name) const
{
	// The player's own missile programs aren't in any data file.
	const string key = kind + " " + name;
	const string playerText = (kind == "guidance") ? MissileGuidance::PlayerProgramText(name)
		: (kind == "role") ? EscortRole::PlayerRoleText(name) : string();
	auto it = sources.find(key);
	if(it != sources.end() && (playerText.empty() || it->second.text == playerText))
		return it->second;

	Source &source = sources[key];
	source = Source();
	if(!playerText.empty())
		source.text = playerText;
	else
		source.text = GameData::ScriptText(kind, name, &source.firstLine);
	istringstream in(source.text);
	for(string line; getline(in, line); )
	{
		int indent = 0;
		size_t i = 0;
		for( ; i < line.size() && (line[i] == '\t' || line[i] == ' '); ++i)
			indent += (line[i] == '\t') ? 4 : 1;
		// Comments leave gaps in the map.
		const bool isComment = (i < line.size() && line[i] == '#');
		source.shape.emplace_back(indent / 4, isComment ? 0 : static_cast<int>(line.size() - i));
	}
	return source;
}



const ScriptWatchDisplay::Entry *ScriptWatchDisplay::Find(const string &key) const
{
	for(const Entry &entry : entries)
		if(entry.key == key)
			return &entry;
	return nullptr;
}



const script::Activity::Run *ScriptWatchDisplay::MainRun(const Entry &entry)
{
	for(const string &kind : entry.key == "flagship" ? FLAGSHIP_KINDS : SHIP_KINDS)
		for(const script::Activity::Run &run : entry.activity.runs)
			if(run.kind == kind)
				return &run;
	return nullptr;
}



void ScriptWatchDisplay::Show(const string &key)
{
	shown = key;
	selectedKind.clear();
	selectedName.clear();
	const Entry *entry = Find(key);
	const script::Activity::Run *run = entry ? MainRun(*entry) : nullptr;
	if(!run)
	{
		auto it = lastRuns.find(key);
		if(it != lastRuns.end())
			run = &it->second;
	}
	if(run)
	{
		selectedKind = run->kind;
		selectedName = run->name;
	}
	ShowSelected();
	Layout();
}



const script::Activity::Run *ScriptWatchDisplay::SelectedRun(bool *isRunning) const
{
	if(isRunning)
		*isRunning = true;
	if(const Entry *entry = Find(shown))
		for(const script::Activity::Run &run : entry->activity.runs)
			if(run.kind == selectedKind && run.name == selectedName)
				return &run;
	if(isRunning)
		*isRunning = false;
	auto it = lastRuns.find(shown);
	if(it != lastRuns.end() && it->second.kind == selectedKind && it->second.name == selectedName)
		return &it->second;
	return nullptr;
}



void ScriptWatchDisplay::ShowSelected()
{
	const Entry *entry = Find(shown);
	// Without a code scanner, or with too much jamming, there is no code to show.
	string text;
	string key;
	if(entry && !entry->unreadable.empty())
	{
		text = "# " + entry->unreadable;
		key = "# " + text;
	}
	else
	{
		key = selectedKind + " " + selectedName;
		const Source &source = GetSource(selectedKind, selectedName);
		text = selectedName.empty() ? "# No scripts are running." : source.text.empty()
			? "# The code of " + selectedKind + " \"" + selectedName + "\" isn't available." : source.text;
	}
	// Each faction's programmers sign their code.
	const CodeStyle *style = entry ? entry->style : nullptr;
	const size_t headerLines = style ? style->Header().size() : 0;
	if(headerLines)
	{
		string header;
		for(const string &line : style->Header())
			header += line + "\n";
		text = header + text;
		key = style->Name() + " " + key;
	}
	editor.SetStyle(style);
	if(key != loadedKey)
	{
		loadedKey = key;
		document.SetText(text);
		editor.SetRun({});
	}
	// Once a script stops running (e.g. its missile is gone), keep showing
	// what it did last.
	if(entry && entry->unreadable.empty())
		if(const script::Activity::Run *run = SelectedRun())
			editor.SetRun(CodeEditor::RunNotes(run->trace, GetSource(selectedKind, selectedName).firstLine,
				headerLines));
	editor.SetGlitch(entry ? Interference(*entry) : string());
}



void ScriptWatchDisplay::Layout()
{
	const double width = min(640., Screen::Width() * .45);
	pane = Rectangle::WithCorners(Screen::TopLeft() + Point(PANE_LEFT, PANE_TOP),
		Point(Screen::Left() + PANE_LEFT + width, Screen::Bottom() - PANE_BOTTOM));
	closeBox = Rectangle::WithCorners(Point(pane.Right() - 24., pane.Top() + 4.),
		Point(pane.Right() - 4., pane.Top() + 24.));
	hackBox = Rectangle::WithCorners(Point(pane.Right() - 150., pane.Top() + 4.),
		Point(pane.Right() - 32., pane.Top() + 24.));
	copyBox = Rectangle::WithCorners(Point(hackBox.Left() - 78., pane.Top() + 4.),
		Point(hackBox.Left() - 8., pane.Top() + 24.));
	editBox = Rectangle::WithCorners(Point(pane.Right() - 150., pane.Top() + 4.),
		Point(pane.Right() - 32., pane.Top() + 24.));
	editor.SetArea(Rectangle::WithCorners(Point(pane.Left(), pane.Top() + HEADER_HEIGHT), pane.BottomRight()));

	// A button for everything being watched, and under them, a tab for each
	// script the one shown ran on the latest step.
	const Font &font = FontSet::Get(14);
	auto row = [&font, this](double y, const vector<string> &labels, auto add) {
		double x = pane.Left() + 10.;
		for(size_t i = 0; i < labels.size(); ++i)
		{
			const double width = font.Width(labels[i]) + 12.;
			if(x + width > pane.Right() - 10.)
				break;
			add(Rectangle::WithCorners(Point(x, y), Point(x + width, y + ROW_HEIGHT - 2.)), i);
			x += width + 6.;
		}
	};
	vector<string> names;
	for(const Entry &entry : entries)
		names.push_back(entry.name);
	entryButtons.clear();
	row(pane.Top() + 28., names, [this](const Rectangle &box, size_t i) {
		entryButtons.emplace_back(box, entries[i].key);
	});
	tabs.clear();
	if(const Entry *entry = Find(shown))
	{
		vector<string> runs;
		for(const script::Activity::Run &run : entry->activity.runs)
			runs.push_back(run.name);
		if(entry->unreadable.empty())
			row(pane.Top() + 50., runs, [this](const Rectangle &box, size_t i) { tabs.emplace_back(box, i); });
	}
}



void ScriptWatchDisplay::DrawMap(const Rectangle &box, const Entry *entry) const
{
	const Font &font = FontSet::Get(14);
	const script::Activity &activity = entry->activity;
	const script::Activity::Run *run = entry->unreadable.empty() ? MainRun(*entry) : nullptr;
	const bool glitching = !Interference(*entry).empty();
	// Without a script, say why: which of the AI's built-in rules it followed,
	// or why its code can't be read.
	string title = !entry->unreadable.empty() ? entry->unreadableLabel : run ? run->name
		: !activity.overriddenBy.empty() ? activity.overriddenBy.substr(0, activity.overriddenBy.find(':'))
		: !activity.notes.empty() ? activity.notes.back() : "idle";
	font.Draw(Fit(font, title, box.Width()), box.TopLeft(), glitching ? GLITCH : LABEL);
	if(!run && !glitching)
		return;

	// One dash for each line of code, lit if the line ran on the latest step.
	const double top = box.Top() + 18.;
	const double height = box.Height() - 18.;
	const uint32_t phase = frame / 4;
	if(!run)
	{
		// Nothing but noise.
		for(int i = 0; i < 12; ++i)
		{
			const uint32_t h = Hash(phase, i);
			const double y = top + (h % 1000) / 1000. * height;
			const double x = box.Left() + h / 1000 % static_cast<uint32_t>(max(1., box.Width() * .6));
			LineShader::Draw(Point(x, y), Point(x + 4. + h % 30, y), 1.f, GLITCH, false);
		}
		return;
	}
	const Source &source = GetSource(run->kind, run->name);
	if(source.shape.empty())
		return;
	vector<bool> ran(source.shape.size());
	for(const script::Trace::Step &step : run->trace.steps)
		if(step.line >= source.firstLine && step.line - source.firstLine < ran.size())
			ran[step.line - source.firstLine] = true;
	// In a tall box, each line is a row, as in the code. In a wide one, the
	// code is turned on its side: each line is a column, longer lines are
	// taller, and indented ones start lower.
	const bool isWide = box.Width() > height;
	const double across = isWide ? box.Width() : height;
	const double along = isWide ? height : box.Width();
	const double step = min(4., across / source.shape.size());
	const float thickness = max(1.f, static_cast<float>(step) - 1.f);
	// Scale the widest line to fit, with a tab as wide as four letters.
	int widest = 1;
	for(const auto &[indent, length] : source.shape)
		widest = max(widest, indent * 4 + length);
	const double charSize = along / widest;
	const double indentSize = 4. * charSize;
	for(size_t i = 0; i < source.shape.size(); ++i)
	{
		const auto &[indent, length] = source.shape[i];
		if(!length)
			continue;
		double start = indent * indentSize;
		const double end = min(along, start + length * charSize);
		if(end <= start)
			continue;
		const double offset = (i + .5) * step;
		const Color *lit = entry->style ? entry->style->GetColor("lit") : nullptr;
		Color color = ran[i] ? (lit ? *lit : LIT_LINE) : DARK_LINE;
		if(glitching && Hash(phase, i) % 3 == 0)
		{
			start += static_cast<int>(Hash(phase, i + 99) % 9) - 4;
			color = GLITCH;
		}
		if(isWide)
			LineShader::Draw(Point(box.Left() + offset, top + start), Point(box.Left() + offset, top + end),
				thickness, color, false);
		else
			LineShader::Draw(Point(box.Left() + start, top + offset), Point(box.Left() + end, top + offset),
				thickness, color, false);
	}
}



void ScriptWatchDisplay::DrawPane() const
{
	const Font &font = FontSet::Get(14);
	const Entry *entry = Find(shown);
	FillShader::Fill(pane.Center(), pane.Dimensions(), PANE);

	// What is shown: whose script, and which.
	string title = entry ? entry->name : "";
	if(entry && entry->unreadable.empty() && !selectedName.empty())
	{
		bool isRunning = false;
		SelectedRun(&isRunning);
		title += ": " + selectedKind + " \"" + selectedName + "\"" + (isRunning ? "" : " (stopped)");
	}
	font.Draw(Fit(font, title, hackBox.Left() - pane.Left() - 20.), pane.TopLeft() + Point(10., 6.), TITLE);
	font.Draw("x", closeBox.Center() - Point(font.Width("x") / 2., 8.), LABEL);

	// With a code injector, someone else's code can be hacked.
	if(entry && !entry->isOwn && injector.isInstalled)
	{
		const string label = armedFrames ? "ARMED " + to_string((armedFrames + 59) / 60) : injector.isReady ? "HACK"
			: injector.status;
		FillShader::Fill(hackBox.Center(), hackBox.Dimensions(), injector.isReady ? HACK_BUTTON : BUTTON);
		const string text = Fit(font, label, hackBox.Width() - 8.);
		font.Draw(text, hackBox.Center() - Point(font.Width(text) / 2., 8.), injector.isReady ? HACK : LABEL);
	}

	// An escort's role can be edited.
	if(entry && !entry->uuid.empty() && selectedKind == "role")
	{
		FillShader::Fill(editBox.Center(), editBox.Dimensions(), editing ? HACK_BUTTON : BUTTON);
		const string label = editing ? "EDITING" : "EDIT";
		font.Draw(label, editBox.Center() - Point(font.Width(label) / 2., 8.), TITLE);
	}

	// With a code scanner, someone else's code can be copied.
	if(entry && !entry->isOwn && entry->unreadable.empty() && (selectedKind == "guidance" || selectedKind == "maneuver"))
	{
		const bool clear = entry->interference.empty();
		FillShader::Fill(copyBox.Center(), copyBox.Dimensions(), BUTTON);
		const string label = "COPY";
		font.Draw(label, copyBox.Center() - Point(font.Width(label) / 2., 8.), clear ? TITLE : LABEL);
	}

	// Everything being watched.
	for(const auto &[box, key] : entryButtons)
	{
		const Entry *other = Find(key);
		if(!other)
			continue;
		const bool isShown = (key == shown);
		FillShader::Fill(box.Center(), box.Dimensions(), isShown ? BUTTON : Color(.03f, .07f, .05f, 1.f));
		font.Draw(other->name, box.TopLeft() + Point(6., 1.), isShown ? SELECTED : LABEL);
	}

	// Every script it ran on the latest step.
	if(entry)
		for(const auto &[box, index] : tabs)
		{
			if(index >= entry->activity.runs.size())
				break;
			const script::Activity::Run &run = entry->activity.runs[index];
			const bool isSelected = (run.kind == selectedKind && run.name == selectedName);
			if(isSelected)
				FillShader::Fill(box.Center(), box.Dimensions(), BUTTON);
			font.Draw(run.name, box.TopLeft() + Point(6., 1.), isSelected ? SELECTED : LABEL);
		}

	// Which of the AI's rules the ship followed.
	string rules;
	if(entry && entry->unreadable.empty())
		for(const string &note : entry->activity.notes)
			rules += (rules.empty() ? "" : "  >  ") + note;
	if(!rules.empty())
		font.Draw(Fit(font, rules, pane.Width() - 20.), pane.TopLeft() + Point(10., 72.), LABEL);

	editor.Draw();
	// While editing a role, say so, and how the last edit went.
	if(!editNote.empty())
	{
		const double height = 26.;
		const Point center(pane.Center().X(), pane.Bottom() - height / 2. - 8.);
		FillShader::Fill(center, Point(pane.Width() - 8., height), Color(.05f, .12f, .2f, .95f));
		const string text = Fit(font, editNote, pane.Width() - 20.);
		font.Draw(text, Point(center.X() - font.Width(text) / 2., center.Y() - 8.), TITLE);
	}
	// While the injector is armed, say how to aim it; afterward, how it went.
	if(armedFrames || resultFrames)
	{
		const string text = armedFrames ? "INJECT: click a line that ran on this step (lit), in "
			+ to_string((armedFrames + 59) / 60) + " s" : injectResult;
		// (Above the bar that says why code is breaking up, which a hack shows.)
		const double height = 26.;
		const Point center(pane.Center().X(), pane.Bottom() - height / 2. - 40.);
		FillShader::Fill(center, Point(pane.Width() - 8., height), Color(.25f, .07f, .04f, .95f));
		font.Draw(text, Point(center.X() - font.Width(text) / 2., center.Y() - 8.), HACK);
		if(armedFrames && (armedFrames / 10) % 2)
		{
			const Rectangle box = editor.Area();
			const Color edge = HACK;
			LineShader::Draw(box.TopLeft(), Point(box.Right(), box.Top()), 1.5f, edge, false);
			LineShader::Draw(Point(box.Left(), box.Bottom()), box.BottomRight(), 1.5f, edge, false);
			LineShader::Draw(box.TopLeft(), Point(box.Left(), box.Bottom()), 1.5f, edge, false);
			LineShader::Draw(Point(box.Right(), box.Top()), box.BottomRight(), 1.5f, edge, false);
		}
	}
}
