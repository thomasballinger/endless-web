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
// from a "maneuver" node in the game data. It decides how far to turn and
// whether to thrust forward or back, or use the afterburner. The AI runs the
// maneuver named "keep station" to hold a ship near a moving target (e.g. an
// escort near its parent), "circle around" to stay close to a target without
// matching its velocity, "move to" to fly to a point and arrive there with a
// given velocity, "pick up" to fly into flotsam to collect it, "attack" to
// close in on a target while aiming at it, and "fight" to decide how to fight
// a ship: keep a safe distance, close in, or just aim.
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
		// Whether the ship has an afterburner and enough fuel, energy and
		// cooling to use it.
		SHOULD_USE_AFTERBURNER,
		// The ship's maximum velocity, counting its afterburner if it should
		// use it.
		TOP_SPEED,
		// For "move to": how close counts as arrived, how slow (relative to the
		// target velocity) counts as stopped, and the speed to cruise at (0 for
		// full speed).
		RADIUS,
		SLOW,
		CRUISE_SPEED,
		// The turn command already given this step.
		COMMAND_TURN,
		// For "attack": the direction to point the ship's guns to hit the
		// target, allowing for the speed of their projectiles.
		AIM,
		// For "fight": whether the ship's personality is to ram its target,
		// the target's top speed, whether the ship fights from a distance
		// with artillery, the range of its shortest-range weapon and of its
		// shortest-range artillery, how far it must stay away from its target
		// to be safe from its own weapons' blasts, the two ships' radii, and
		// the ship's top speed and acceleration in reverse.
		RAMMING,
		TARGET_MAX_VELOCITY,
		ARTILLERY,
		SHORTEST_RANGE,
		SHORTEST_ARTILLERY,
		MIN_SAFE_DISTANCE,
		SHIP_RADIUS,
		TARGET_RADIUS,
		MAX_REVERSE_VELOCITY,
		REVERSE_ACCELERATION,
		// Outputs: the turn command (in units of the ship's turn rate, starting
		// at COMMAND_TURN), whether to thrust forward, in reverse, or with the
		// afterburner, and for "move to", whether the ship has arrived.
		TURN,
		FORWARD,
		BACK,
		AFTERBURNER,
		ARRIVED
	};
	// Actions a maneuver can ask for. Only "fight" uses them: "attack" flies
	// the "attack" maneuver, and "aim-guns" only turns to aim at the target.
	enum Effect {
		ATTACK,
		AIM_GUNS
	};

	// Calculations about the ship being flown, which scripts call as
	// functions. The AI provides these for a real ship.
	class Context {
	public:
		virtual ~Context() = default;
		// Where the ship would come to rest relative to something moving at the
		// given velocity, and whether reverse thrust gets it there sooner.
		virtual Point StoppingPoint(const Point &targetVelocity, bool &shouldReverse) const = 0;
		// The turn command that points the ship along the given vector.
		virtual double TurnToward(const Point &vector) const = 0;
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
		bool shouldUseAfterburner = false;
		double topSpeed = 0.;
		double radius = 0.;
		double slow = 0.;
		double cruiseSpeed = 0.;
		double commandTurn = 0.;
		Point aim;
		bool ramming = false;
		double targetMaxVelocity = 0.;
		bool artillery = false;
		double shortestRange = 0.;
		double shortestArtillery = 0.;
		double minSafeDistance = 0.;
		double shipRadius = 0.;
		double targetRadius = 0.;
		double maxReverseVelocity = 0.;
		double reverseAcceleration = 0.;
	};
	// What the script decided.
	class Output {
	public:
		double turn = 0.;
		bool forward = false;
		bool back = false;
		bool afterburner = false;
		bool arrived = false;
		bool attack = false;
		bool aimGuns = false;
	};

	static const script::Interface &GetInterface();


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool IsValid() const;

	// Run the script. Returns false, with no command, if the script fails.
	// Scripts that call the ship functions need a context.
	bool Run(const Input &input, Output &output, const Context *context = nullptr) const;


private:
	script::Program program;
};
