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

class CodeStyle;
class Point;



// Shows the player the scripts that ships and missiles are running. In the
// HUD, the flagship and its target each get a small map of the code of the
// script that matters most at the moment (a ship's maneuver, or a missile's
// guidance), with the lines that ran on the latest step lit up. Clicking one
// (or the "Show scripts" key) opens a pane beside the HUD with that script's
// code, showing which lines ran and what they made, a tab for every script
// that ran, and the AI rules followed. Buttons along the top of the pane
// switch between everything being watched: the flagship, its target, the
// selected escorts and the missile closest to hitting the flagship.
//
// Other ships' code can only be read with a code scanner, within its range,
// and a code jammer garbles it or hides it entirely. With a code injector,
// the pane can hack what it shows.
class ScriptWatchDisplay {
public:
	// One ship or missile whose scripts the player is watching.
	class Entry {
	public:
		// Which one it is: "flagship", "target", "escort 1"... or "incoming".
		std::string key;
		std::string name;
		// Whether it is the player's own (so its code can always be read, and
		// it can't be hacked).
		bool isOwn = false;
		// Why its code can't be read, or empty if it can, and a shorter way
		// of saying so, for the HUD.
		std::string unreadable;
		std::string unreadableLabel;
		// Jamming that garbles its code, if any.
		std::string interference;
		// How its faction's code looks, or null for the usual way.
		const CodeStyle *style = nullptr;
		script::Activity activity;
	};

	// What the player's code injector can do at the moment.
	class Injector {
	public:
		bool isInstalled = false;
		bool isReady = false;
		// "ready", or why it isn't.
		std::string status;
	};


public:
	ScriptWatchDisplay();

	// Take what the watched scripts did on the latest step.
	void Update(const std::vector<Entry> &entries, const Injector &injector);

	// Draw the code maps in the given parts of the HUD, and the pane if open.
	void Draw(const Rectangle &flagshipBox, const Rectangle &targetBox) const;

	// Open the pane on the target (or the flagship), or close it.
	void Toggle();
	// Handle a click or scroll, returning false if it wasn't on this display.
	// A click on the pane's hack button sets which entry to hack.
	bool Click(const Point &point, std::string &hack);
	bool Scroll(const Point &point, double dy);
	// Which entry the pane shows, if it is open.
	const std::string &Shown() const;

	// Arm the code injector on what the pane shows: for a few seconds, a click
	// on a line of its code that ran on the latest step injects there (and
	// hacks it); a click on one that didn't misses. Returns false if the pane
	// isn't showing something the injector can hack.
	bool Arm();
	// Count down an armed injector, once a frame.
	void Step();
	// If the player clicked the pane's copy button since this was last asked,
	// the code they copied (the text of a "guidance" or "maneuver" node), and
	// whose it was.
	bool TakeCopy(std::string &text, std::string &from);


private:
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
	const Entry *Find(const std::string &key) const;
	// The run worth showing first: what decided where a ship goes, or how a
	// missile steers (the flagship's missiles come first, since the player
	// flies the flagship).
	static const script::Activity::Run *MainRun(const Entry &entry);
	// Open the pane on the given entry.
	void Show(const std::string &key);
	// The selected script's run on the latest step, or if it didn't run, its
	// last run (if it was the one that mattered most), or null.
	const script::Activity::Run *SelectedRun(bool *isRunning = nullptr) const;
	// Put the selected script in the editor, and what it did beside its lines.
	void ShowSelected();
	void Layout();

	void DrawMap(const Rectangle &box, const Entry *entry) const;
	void DrawPane() const;


private:
	std::vector<Entry> entries;
	Injector injector;
	// While the injector is armed, frames left to aim it; then, for a while, how
	// the injection went.
	int armedFrames = 0;
	std::string injectResult;
	int resultFrames = 0;
	// The last run of each entry's most important script, kept so the pane
	// can show it after it stops (e.g. when a missile is gone).
	std::map<std::string, script::Activity::Run> lastRuns;
	std::map<std::string, std::string> lastNames;

	// The entry the pane shows, or empty if it is closed, and its script.
	std::string shown;
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
	Rectangle hackBox;
	Rectangle copyBox;
	// Code the player copied, until the engine takes it.
	std::string copiedText;
	std::string copiedFrom;
	std::vector<std::pair<Rectangle, std::string>> entryButtons;
	std::vector<std::pair<Rectangle, size_t>> tabs;

	mutable std::map<std::string, Source> sources;
	mutable int frame = 0;
};
