/* ScriptWatchDisplay.h
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

#pragma once

#include "CodeDocument.h"
#include "CodeEditor.h"
#include "Rectangle.h"
#include "script/Script.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

class Point;



// Shows the player the scripts their flagship and its target are running. In
// the HUD, each gets a small map of the code of the script that matters most
// at the moment (a ship's maneuver, or a missile's guidance), with the lines
// that ran on the latest step lit up. Clicking one opens a pane beside the
// HUD with that script's code, showing which lines ran and what they made,
// with a tab for every script the ship ran and the AI rules it followed.
class ScriptWatchDisplay {
public:
	ScriptWatchDisplay();

	// Take what the scripts did on the latest step. The target is null if the
	// flagship has no target ship.
	void Update(const script::Activity &flagship, const script::Activity *target, const std::string &targetName);

	// Draw the code maps in the given parts of the HUD, and the pane if open.
	void Draw(const Rectangle &flagshipBox, const Rectangle &targetBox) const;

	// Handle a click or scroll, returning false if it wasn't on this display.
	bool Click(const Point &point);
	bool Scroll(const Point &point, double dy);


private:
	enum class Shown { NONE, FLAGSHIP, TARGET };

	// What a script's source looks like, for drawing its map.
	class Source {
	public:
		std::string text;
		// The data file's line number for the first line of the text.
		size_t firstLine = 1;
		// Each line's indentation and length, in characters.
		std::vector<std::pair<int, int>> shape;
	};


private:
	const Source &GetSource(const std::string &kind, const std::string &name) const;
	// The run worth showing first: what decided where the ship goes, or how
	// its missile steers.
	static const script::Activity::Run *MainRun(const script::Activity &activity);
	const script::Activity &ShownActivity() const;
	// The selected script's run on the latest step, or if it didn't run, its
	// last run (if it was the one that mattered most), or null.
	const script::Activity::Run *SelectedRun(bool *isRunning = nullptr) const;
	// Put the selected script in the editor, and what it did beside its lines.
	void ShowSelected();
	void Layout();

	void DrawMap(const Rectangle &box, const script::Activity &activity) const;
	void DrawPane() const;


private:
	script::Activity flagship;
	script::Activity target;
	// The last run of each one's most important script, kept so the pane can
	// show it after it stops (e.g. when a missile is gone).
	script::Activity::Run lastFlagshipRun;
	script::Activity::Run lastTargetRun;
	bool hasTarget = false;
	std::string targetName;

	Shown shown = Shown::NONE;
	// The script shown in the pane.
	std::string selectedKind;
	std::string selectedName;
	CodeDocument document;
	CodeEditor editor;
	std::string loadedKey;
	// The values beside the code change too fast to read every frame.
	int updates = 0;

	// Where things were last drawn, for clicks.
	mutable Rectangle flagshipArea;
	mutable Rectangle targetArea;
	Rectangle pane;
	Rectangle closeBox;
	std::vector<std::pair<Rectangle, size_t>> tabs;

	mutable std::map<std::string, Source> sources;
	mutable int frame = 0;
};
