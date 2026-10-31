/* MissileGuidance.h
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

#include <map>
#include <string>

class DataNode;
class Weapon;



// A script that steers a homing projectile each step, loaded from a
// "guidance" node in the game data.
//
// What a script may do depends on the weapon's homing attributes, which act as
// the capabilities of its guidance hardware:
//  - Only "leading" weapons can measure their target's velocity.
//  - Only weapons with "throttle control" can change their thrust.
//  - A "blindspot" seeker cannot see behind the projectile. That is enforced
//    by the game before the script runs: the projectile loses its target.
// Each combination of capabilities has a default script that reproduces the
// game's built-in behavior. A weapon may name another script with a
// "guidance" attribute, which is used only if the weapon has every capability
// the script needs.
//
// The script only decides how to steer. Whether the seeker has a lock is still
// decided by the weapon's tracking attributes, and is given to the script as
// an input. A seeker that loses its lock may also be confused (by jamming, or
// by poor tracking). That is the seeker's doing, not the script's: like a
// blind spot, it is handled before the script runs. A confused projectile
// turns as fast as it can, one way or the other, and its script does not run.
class MissileGuidance {
public:
	// Variables available to guidance scripts, in the order they are added to
	// the script interface. Everything before TURN is read-only.
	enum Variable {
		// The projectile's own position and velocity, and the unit vector it faces.
		POSITION,
		VELOCITY,
		FACING,
		// Whether the seeker can see its target, and where the target is. The
		// target's position and velocity are zero if it cannot.
		HAS_LOCK,
		TARGET_POSITION,
		TARGET_VELOCITY,
		// The weapon's attributes.
		MAX_TURN,
		ACCELERATION,
		DRAG,
		// How many steps since the projectile was fired, and how many until it
		// expires.
		AGE,
		LIFETIME_LEFT,
		// The unit vector the target faces. Seeing it takes an optical seeker,
		// which sees the target's shape. Zero without a lock.
		TARGET_FACING,
		// Worked out from the others, so that scripts can be about what to do
		// rather than arithmetic (each exactly as the default scripts used to
		// work it out). The projectile's top speed:
		TOP_SPEED,
		// While it has a lock: the offset from it to the target, the distance,
		// and the unit vector toward it; the degrees to turn to face it (from
		// -90 to 90: a target behind is 0); and how many steps it would take to
		// get there at top speed.
		TARGET_OFFSET,
		TARGET_DISTANCE,
		TARGET_DIRECTION,
		TARGET_BEARING,
		TARGET_STEPS_AWAY,
		// With "leading": the unit vector toward where to meet the target, and
		// the steps it would take to get there.
		TARGET_INTERCEPT,
		TARGET_INTERCEPT_STEPS,
		// Outputs: degrees to turn this step, and acceleration to apply. They
		// start at the weapon's maximum turn rate and acceleration, and are
		// limited to those after the script runs.
		TURN,
		THRUST
	};
	enum Effect {
		// Forget the current target.
		DROP_TARGET
	};

	// What the projectile knows when its script runs.
	class Input {
	public:
		Point position;
		Point velocity;
		Point facing;
		bool hasLock = false;
		Point targetPosition;
		Point targetVelocity;
		double maxTurn = 0.;
		double acceleration = 0.;
		double drag = 0.;
		int age = 0;
		int lifetimeLeft = 0;
		Point targetFacing;
	};
	// What the script decided.
	class Output {
	public:
		double turn = 0.;
		double thrust = 0.;
		bool dropTarget = false;
	};

	// One run of a script, kept so that another version of the script can be
	// checked against it: did it decide the same thing, given the same input?
	class Record {
	public:
		// The step it ran on, as numbered by whoever keeps the records.
		int step = 0;
		Input input;
		bool succeeded = false;
		Output output;
		// The random numbers it drew.
		script::RandomTape draws;
	};

	static const script::Interface &GetInterface();
	// What each input, output and function a guidance script can use is, and
	// how it is worked out (in the script language), by its infix name.
	class Doc {
	public:
		std::string text;
		std::string source;
	};
	static const std::map<std::string, Doc> &Documentation();
	// What a script is given for the given input: a value for each of the
	// interface's variables (inputs, then outputs at their starting values).
	static void Variables(const Input &input, std::vector<script::Value> &variables);

	// The script that reproduces the built-in behavior of the given weapon.
	static const MissileGuidance *Default(const Weapon &weapon);
	// The name of the default script for a weapon with the given capabilities.
	static std::string DefaultName(bool leading, bool throttleControl);

	// The player can program the guidance of their own ships' homing weapons
	// (see WorkshopPanel). Programs are given as the text of a "guidance"
	// node, and are kept compiled here, by weapon, for projectiles to use.
	// Setting a program that doesn't compile, or an empty one, clears it.
	// Returns whether the weapon now has a program.
	static bool SetPlayerProgram(const Weapon *weapon, const std::string &text);
	static const MissileGuidance *PlayerProgram(const Weapon *weapon);
	// The text of the player's program with the given name, or empty.
	static std::string PlayerProgramText(const std::string &name);
	static void ClearPlayerPrograms();


public:
	// Load a script. With logErrors false (e.g. while a player is typing it),
	// errors are only kept in Program().Errors().
	void Load(const DataNode &node, const std::string &name, bool logErrors = true);

	const std::string &Name() const;
	bool IsValid() const;
	const script::Program &Program() const;

	// Describe a capability this script uses that the weapon lacks, or return
	// an empty string if the weapon may use this script.
	std::string MissingCapability(const Weapon &weapon) const;
	std::string MissingCapability(bool leading, bool throttleControl, bool optical = false) const;

	// Run the script. The turn and thrust are limited to what the weapon can
	// do. Returns false, with no turn, if the script fails.
	// The memory holds the script's "memory" values for one projectile.
	bool Run(const Input &input, Output &output, script::Memory *memory = nullptr,
		script::Trace *trace = nullptr) const;


private:
	std::string name;
	script::Program program;
};
