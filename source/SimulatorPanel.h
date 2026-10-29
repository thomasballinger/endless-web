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

#include "GuidanceSimulation.h"
#include "Point.h"

#include <memory>

class Outfit;
class Ship;



// A practice range for guidance scripts, set apart from the game itself: a
// grid instead of stars and planets, where a launcher fires a homing weapon
// at a target flying a drill. Time can be paused, stepped, sped up, and
// replayed from the start, and clicking a projectile shows how its guidance
// script ran on the latest step.
class SimulatorPanel : public Panel {
public:
	SimulatorPanel(const Outfit &outfit, const Ship &target);

	virtual void Step() override;
	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;
	virtual bool Scroll(double dx, double dy) override;


private:
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

	bool paused = false;
	// Steps per frame: 1/4, 1/2, 1, 2, 4 or 8.
	int speedIndex = 2;
	int frame = 0;

	Point center;
	double zoom = .5;
	// The zoom chosen with the mouse wheel, relative to the automatic one.
	double zoomFactor = 1.;
};
