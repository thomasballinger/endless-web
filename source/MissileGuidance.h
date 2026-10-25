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

#include "script/Script.h"

#include <string>

class DataNode;



// A script that steers a homing projectile each step, loaded from a
// "guidance" node in the game data. A weapon names its guidance program with
// a "guidance" attribute; homing weapons without one use "default".
//
// The script only decides how to steer. Whether the seeker has a lock or is
// confused is still decided by the weapon's tracking attributes, and is given
// to the script as an input.
class MissileGuidance {
public:
	// Variables available to guidance scripts, in the order they are added to
	// the script interface. Everything before TURN is read-only.
	enum Variable {
		// The projectile's own position and velocity, and the unit vector it faces.
		POSITION,
		VELOCITY,
		FACING,
		// Whether the projectile has a target, and where it is.
		HAS_TARGET,
		TARGET_POSITION,
		TARGET_VELOCITY,
		// The state of the seeker.
		HAS_LOCK,
		CONFUSED,
		// Which way a confused projectile turns: 1 or -1.
		CONFUSION_DIRECTION,
		// The weapon's attributes.
		MAX_TURN,
		ACCELERATION,
		DRAG,
		BLINDSPOT,
		LEADING,
		THROTTLE_CONTROL,
		// Outputs: degrees to turn this step, and acceleration to apply. They
		// start at the weapon's maximum turn rate and acceleration.
		TURN,
		THRUST
	};
	enum Effect {
		// Forget the current target.
		DROP_TARGET
	};

	static const script::Interface &GetInterface();


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool IsValid() const;
	const script::Program &Program() const;


private:
	script::Program program;
};
