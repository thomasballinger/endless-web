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



// A path for a target to fly while homing weapons are fired at it, for
// testing guidance scripts. The launcher is at the origin facing "up"
// (negative y); the target starts some distance away and moves at some speed.
class GuidanceDrill {
public:
	// Where the target is on a given step.
	class Motion {
	public:
		Point position;
		Point velocity;
		Angle facing;
	};


public:
	// The drills: ahead holding still, fleeing, approaching, crossing,
	// jinking and circling, then to the side and behind, holding still.
	static std::vector<GuidanceDrill> MakeDrills(double distance, double speed);


public:
	std::string name;
	// The target's motion on the given step (step 0 is when it starts).
	std::function<Motion(int step)> motion;
};
