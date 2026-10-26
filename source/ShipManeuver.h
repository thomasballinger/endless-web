/* ShipManeuver.h
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

#include "Point.h"
#include "script/Script.h"

#include <string>

class DataNode;



// A script that flies a ship relative to another object for one step, loaded
// from a "maneuver" node in the game data. The AI runs the maneuver named
// "keep station" to hold a ship near a moving target (e.g. an escort near its
// parent); it decides how far to turn and whether to thrust forward or back.
class ShipManeuver {
public:
	// Variables available to maneuver scripts, in the order they are added to
	// the script interface. Everything before TURN is read-only.
	enum Variable {
		// The ship's own position and velocity, the unit vector it faces, and
		// the direction it faces in degrees (as the game's angles round it).
		POSITION,
		VELOCITY,
		FACING,
		FACING_DEGREES,
		// The object to keep station on.
		TARGET_POSITION,
		TARGET_VELOCITY,
		TARGET_FACING,
		// The ship's flight characteristics.
		MAX_VELOCITY,
		ACCELERATION,
		TURN_RATE,
		INERTIAL_MASS,
		DRAG_FORCE,
		REVERSE_THRUST,
		// The turn command the ship gave on the last step, from -1 to 1.
		LAST_TURN,
		// Outputs: the turn command (in units of the ship's turn rate), and
		// whether to thrust forward or in reverse. They start at 0 and false.
		TURN,
		FORWARD,
		BACK
	};

	// What the ship knows when the script runs.
	class Input {
	public:
		Point position;
		Point velocity;
		Point facing;
		double facingDegrees = 0.;
		Point targetPosition;
		Point targetVelocity;
		Point targetFacing;
		double maxVelocity = 0.;
		double acceleration = 0.;
		double turnRate = 0.;
		double inertialMass = 0.;
		double dragForce = 0.;
		double reverseThrust = 0.;
		double lastTurn = 0.;
	};
	// What the script decided.
	class Output {
	public:
		double turn = 0.;
		bool forward = false;
		bool back = false;
	};

	static const script::Interface &GetInterface();


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool IsValid() const;

	// Run the script. Returns false, with no command, if the script fails.
	bool Run(const Input &input, Output &output) const;


private:
	script::Program program;
};
