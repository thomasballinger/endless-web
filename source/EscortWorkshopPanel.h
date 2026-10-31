/* EscortWorkshopPanel.h
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
#include "EscortSimulation.h"
#include "Point.h"
#include "Rectangle.h"
#include "ShipManeuver.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class PlayerInfo;
class Ship;



// Where the player programs how their escorts fly: the "keep station"
// maneuver, which holds an escort near its leader. A leader flies a drill (or
// the player flies it, with the arrow keys) and an escort keeps station on
// it, with the program being edited. Every edit replays the run so far with
// the new program, the old escort's course showing faintly. Installing the
// program makes the player's escorts fly it.
class EscortWorkshopPanel : public Panel {
public:
	EscortWorkshopPanel(PlayerInfo *player, const Ship &leader, const Ship &escort);

	virtual void Step() override;
	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool TextInput(const std::string &text) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Release(int x, int y, MouseButton button) override;
	virtual bool Scroll(double dx, double dy) override;


private:
	void Recompile();
	void Install();
	std::string FactoryText() const;
	void NextEscort();
	void Layout();
	Point ToScreen(const Point &position) const;
	void DrawGrid() const;
	void DrawInspector() const;
	void DrawStatus() const;
	void ScrubTo(double x);


private:
	PlayerInfo *player = nullptr;
	const Ship *leaderModel = nullptr;
	std::unique_ptr<EscortSimulation> simulation;
	// The program in use, and the version of the document it came from.
	ShipManeuver program;
	CodeDocument document;
	CodeEditor editor;
	uint64_t compiledVersion = 0;
	uint64_t seenVersion = 0;
	int quietFrames = 0;
	bool hasErrors = false;
	// The escort's course before the last edit, shown faintly.
	std::deque<Point> ghost;
	std::string status;
	int statusTime = 0;
	bool paused = false;
	bool wasFlying = false;
	bool justFocused = false;
	int escortIndex = 0;

	Rectangle view;
	Point center;
	double zoom = .4;
	Point dragPoint;
	bool isDragging = false;
	bool isScrubbing = false;
};
