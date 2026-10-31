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

#include "Color.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "shader/LineShader.h"
#include "Point.h"
#include "Screen.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {
	// Which kinds of script say the most about what an object is doing, best
	// first: how its missile steers, or how the ship is flying.
	const vector<string> MAIN_KINDS = {"guidance", "maneuver", "choose", "rate", "turret"};
	// The code in the pane changes this often, in frames, so it can be read.
	const int UPDATE_FRAMES = 6;
	// The pane sits beside the HUD's radar and target display.
	const double PANE_LEFT = 270.;
	const double PANE_TOP = 70.;
	const double PANE_BOTTOM = 170.;
	const double HEADER_HEIGHT = 66.;

	const Color LABEL(.40f, .55f, .45f, 1.f);
	const Color DARK_LINE(.16f, .24f, .19f, .8f);
	const Color LIT_LINE(.35f, .90f, .50f, 1.f);
	const Color GLITCH(.95f, .40f, .95f, 1.f);
	const Color PANE(.01f, .035f, .02f, .92f);
	const Color TITLE(.75f, .95f, .80f, 1.f);
	const Color SELECTED(.55f, .80f, 1.f, 1.f);

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
}



ScriptWatchDisplay::ScriptWatchDisplay()
	: editor(document)
{
}



void ScriptWatchDisplay::Update(const script::Activity &flagship, const script::Activity *target,
	const string &targetName)
{
	this->flagship = flagship;
	hasTarget = target;
	this->target = target ? *target : script::Activity();
	this->targetName = targetName;
	if(shown == Shown::TARGET && !hasTarget)
		shown = Shown::NONE;
	if(shown == Shown::NONE)
		return;

	// Show what matters most, until the player picks something else.
	if(selectedName.empty())
		if(const script::Activity::Run *run = MainRun(ShownActivity()))
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
	flagshipArea = flagshipBox;
	DrawMap(flagshipBox, flagship);
	targetArea = hasTarget ? targetBox : Rectangle();
	if(hasTarget)
		DrawMap(targetBox, target);
	if(shown != Shown::NONE)
		DrawPane();
}



bool ScriptWatchDisplay::Click(const Point &point)
{
	if(shown != Shown::NONE && pane.Contains(point))
	{
		if(closeBox.Contains(point))
			shown = Shown::NONE;
		for(const auto &[box, index] : tabs)
			if(box.Contains(point) && index < ShownActivity().runs.size())
			{
				selectedKind = ShownActivity().runs[index].kind;
				selectedName = ShownActivity().runs[index].name;
				ShowSelected();
				Layout();
			}
		return true;
	}

	Shown clicked = Shown::NONE;
	if(flagshipArea.Contains(point))
		clicked = Shown::FLAGSHIP;
	else if(hasTarget && targetArea.Contains(point))
		clicked = Shown::TARGET;
	else
		return false;

	// Clicking the code of what is shown closes it again.
	if(clicked == shown)
	{
		shown = Shown::NONE;
		return true;
	}
	shown = clicked;
	selectedKind.clear();
	selectedName.clear();
	if(const script::Activity::Run *run = MainRun(ShownActivity()))
	{
		selectedKind = run->kind;
		selectedName = run->name;
	}
	ShowSelected();
	Layout();
	return true;
}



bool ScriptWatchDisplay::Scroll(const Point &point, double dy)
{
	if(shown == Shown::NONE || !pane.Contains(point))
		return false;
	editor.Scroll(dy);
	return true;
}



const ScriptWatchDisplay::Source &ScriptWatchDisplay::GetSource(const string &kind, const string &name) const
{
	const string key = kind + " " + name;
	auto it = sources.find(key);
	if(it != sources.end())
		return it->second;

	Source &source = sources[key];
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



const script::Activity::Run *ScriptWatchDisplay::MainRun(const script::Activity &activity)
{
	for(const string &kind : MAIN_KINDS)
		for(const script::Activity::Run &run : activity.runs)
			if(run.kind == kind)
				return &run;
	return nullptr;
}



const script::Activity &ScriptWatchDisplay::ShownActivity() const
{
	return shown == Shown::TARGET ? target : flagship;
}



const script::Activity::Run *ScriptWatchDisplay::SelectedRun() const
{
	for(const script::Activity::Run &run : ShownActivity().runs)
		if(run.kind == selectedKind && run.name == selectedName)
			return &run;
	return nullptr;
}



void ScriptWatchDisplay::ShowSelected()
{
	const string key = selectedKind + " " + selectedName;
	const Source &source = GetSource(selectedKind, selectedName);
	if(key != loadedKey)
	{
		loadedKey = key;
		document.SetText(selectedName.empty() ? "# No scripts are running." : source.text.empty()
			? "# The code of " + selectedKind + " \"" + selectedName + "\" isn't available." : source.text);
	}
	const script::Activity::Run *run = SelectedRun();
	editor.SetRun(run ? CodeEditor::RunNotes(run->trace, source.firstLine) : map<size_t, CodeEditor::LineNote>());
	editor.SetGlitch(ShownActivity().overriddenBy);
}



void ScriptWatchDisplay::Layout()
{
	const double width = min(640., Screen::Width() * .45);
	pane = Rectangle::WithCorners(Screen::TopLeft() + Point(PANE_LEFT, PANE_TOP),
		Point(Screen::Left() + PANE_LEFT + width, Screen::Bottom() - PANE_BOTTOM));
	closeBox = Rectangle::WithCorners(Point(pane.Right() - 24., pane.Top() + 4.),
		Point(pane.Right() - 4., pane.Top() + 24.));
	editor.SetArea(Rectangle::WithCorners(Point(pane.Left(), pane.Top() + HEADER_HEIGHT), pane.BottomRight()));

	// A tab for each script that ran on the latest step.
	tabs.clear();
	const Font &font = FontSet::Get(14);
	double x = pane.Left() + 10.;
	const double y = pane.Top() + 26.;
	const vector<script::Activity::Run> &runs = ShownActivity().runs;
	for(size_t i = 0; i < runs.size(); ++i)
	{
		const double width = font.Width(runs[i].name) + 12.;
		if(x + width > pane.Right() - 10.)
			break;
		tabs.emplace_back(Rectangle::WithCorners(Point(x, y), Point(x + width, y + 18.)), i);
		x += width + 6.;
	}
}



void ScriptWatchDisplay::DrawMap(const Rectangle &box, const script::Activity &activity) const
{
	const Font &font = FontSet::Get(14);
	const script::Activity::Run *run = MainRun(activity);
	const bool glitching = !activity.overriddenBy.empty();
	// Without a script, say which of the AI's built-in rules it followed.
	string title = run ? run->name : glitching ? "confused" : !activity.notes.empty() ? activity.notes.back() : "idle";
	font.Draw(Fit(font, title, box.Width()), box.TopLeft(), glitching ? GLITCH : LABEL);
	if(!run && !glitching)
		return;

	// One dash for each line of code, lit if the line ran on the latest step.
	const double top = box.Top() + 18.;
	const double height = box.Height() - 18.;
	const uint32_t phase = frame / 4;
	if(!run)
	{
		// The seeker is steering: only noise.
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
		Color color = ran[i] ? LIT_LINE : DARK_LINE;
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
	const script::Activity &activity = ShownActivity();
	FillShader::Fill(pane.Center(), pane.Dimensions(), PANE);

	// What is shown: whose script, and which.
	const string who = shown == Shown::TARGET ? targetName : "Your ship";
	string title = who;
	if(!selectedName.empty())
		title += ": " + selectedKind + " \"" + selectedName + "\"" + (SelectedRun() ? "" : " (not running)");
	font.Draw(Fit(font, title, pane.Width() - 40.), pane.TopLeft() + Point(10., 6.), TITLE);
	font.Draw("x", closeBox.Center() - Point(font.Width("x") / 2., 8.), LABEL);

	// Every script that ran on the latest step.
	for(const auto &[box, index] : tabs)
	{
		if(index >= activity.runs.size())
			break;
		const script::Activity::Run &run = activity.runs[index];
		const bool isSelected = (run.kind == selectedKind && run.name == selectedName);
		if(isSelected)
			FillShader::Fill(box.Center(), box.Dimensions(), Color(.08f, .16f, .22f, 1.f));
		font.Draw(run.name, box.TopLeft() + Point(6., 1.), isSelected ? SELECTED : LABEL);
	}

	// Which of the AI's rules the ship followed.
	string rules;
	for(const string &note : activity.notes)
		rules += (rules.empty() ? "" : "  >  ") + note;
	if(!rules.empty())
		font.Draw(Fit(font, rules, pane.Width() - 20.), pane.TopLeft() + Point(10., 46.), LABEL);

	editor.Draw();
}
