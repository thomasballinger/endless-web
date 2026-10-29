/* SimulatorPanel.h
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

#include "Panel.h"

#include "CodeDocument.h"
#include "CodeEditor.h"
#include "GuidanceSimulation.h"
#include "MissileGuidance.h"
#include "Point.h"
#include "Rectangle.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class Outfit;
class Ship;



// A practice range for guidance scripts, set apart from the game itself: a
// grid instead of stars and planets, where a launcher fires a homing weapon
// at a target flying a drill. The weapon's guidance script is open in an
// editor beside it. Every change that compiles replays the range from the
// start to the current moment with the new script, so its effect shows at
// once; mistakes are marked on their lines instead. Time can be paused,
// stepped, sped up, and restarted, and clicking a projectile shows, in the
// editor, which lines of its script ran on the latest step and what they
// produced.
class SimulatorPanel : public Panel {
public:
	SimulatorPanel(const Outfit &outfit, const Ship &target);

	virtual void Step() override;
	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool TextInput(const std::string &text) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Scroll(double dx, double dy) override;


private:
	// Compile the editor's text, and if it compiles, use it and replay.
	void Recompile();
	// Show the selected projectile's latest run in the editor.
	void UpdateRunNotes();
	void Save();
	void Layout();

	// Convert between positions in the simulation and on the screen.
	Point ToScreen(const Point &position) const;
	Point ToWorld(const Point &screen) const;
	void DrawGrid() const;
	void DrawTrails() const;
	void DrawSelection() const;
	void DrawInspector() const;
	void DrawStatus() const;


private:
	const Outfit &outfit;
	std::unique_ptr<GuidanceSimulation> simulation;

	// The script being edited, the version of it in use, and the last
	// version of the text that was compiled.
	CodeDocument document;
	CodeEditor editor;
	MissileGuidance guidance;
	std::string guidanceName;
	uint64_t compiledVersion = 0;
	// Whether the text has mistakes, so an older version is still running.
	bool hasErrors = false;
	std::string status;
	int statusTime = 0;
	// Where the script came from, and where saving it puts it.
	std::filesystem::path sourcePath;

	bool paused = false;
	// Steps per frame: 1/4, 1/2, 1, 2, 4 or 8.
	int speedIndex = 2;
	int frame = 0;

	// The part of the screen showing the range.
	Rectangle view;
	Point center;
	double zoom = .5;
	// The zoom chosen with the mouse wheel, relative to the automatic one.
	double zoomFactor = 1.;
	// Where a drag in the editor is.
	Point dragPoint;
	bool isDragging = false;
};
