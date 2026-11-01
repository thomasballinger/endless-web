/* GuidanceDrill.h
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

#include "Angle.h"
#include "Point.h"

#include <functional>
#include <string>
#include <vector>



// A way for a target to fly while homing weapons are fired at it, for
// testing guidance scripts. The launcher starts at the origin facing "up"
// (negative y); the target starts some distance away. Either the target's
// motion is given for each step, or a pilot flies it, as ships fly: turning
// at its turn rate, and thrusting against its drag.
class GuidanceDrill {
public:
	// Where a ship is, and how it is moving.
	class Motion {
	public:
		Point position;
		Point velocity;
		Angle facing;
	};

	// What a pilot sees: its own ship, what it can do, and the missiles.
	class Situation {
	public:
		int step = 0;
		Motion ship;
		// Degrees per step, and speed added per step at full thrust.
		double turnRate = 0.;
		double acceleration = 0.;
		Point launcher;
		// The position and velocity of each missile in flight.
		std::vector<Motion> missiles;
	};

	// How a pilot flies, each step: how much of its turn rate to turn (from
	// -1, counterclockwise, to 1), and how much of its thrust to use (0 to 1).
	class Control {
	public:
		double turn = 0.;
		double thrust = 0.;
	};


public:
	// The drills: ahead holding still, fleeing, approaching, crossing,
	// jinking and circling, then to the side and behind, holding still.
	static std::vector<GuidanceDrill> MakeDrills(double distance, double speed);
	// Drills with a pilot: evading missiles, cruising, and breaking away.
	static std::vector<GuidanceDrill> MakeFlownDrills(double distance);
	// Move a ship for one step, as the game does: turn, thrust, then drag,
	// which keeps it under its top speed.
	static void Fly(Motion &motion, const Control &control, double turnRate, double acceleration, double topSpeed);


public:
	std::string name;
	// The target's motion on the given step (step 0 is when it starts).
	std::function<Motion(int step)> motion;
	// Or, where the target starts, and its pilot.
	Motion start;
	std::function<Control(const Situation &situation)> pilot;
	// Or, if not empty, the target is flown by the game's own AI (from where
	// it starts), with this personality (see AIWorld).
	std::string ai;
};
