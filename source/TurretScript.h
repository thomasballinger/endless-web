/* TurretScript.h
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



// A script that moves one turret (or one moving decoration) for one step,
// loaded from a "turret" node in the game data. The AI runs the one named
// "idle sweep" for each turret that has nothing to aim at, so that it sweeps
// back and forth around its idle angle.
class TurretScript {
public:
	// Variables available to turret scripts, in the order they are added to
	// the script interface. Everything before AIM is read-only.
	enum Variable {
		// The aim command the turret was given last step, from -1 to 1 (in
		// units of its turn rate). Zero means it was at rest.
		PREVIOUS_AIM,
		// How many degrees the turret must turn to point at its idle angle.
		IDLE_OFFSET,
		// The middle of the turret's arc, in degrees (180 for a turret that
		// can turn all the way around).
		ARC_MIDDLE,
		// Output: the aim command for this step, from -1 to 1. It starts at
		// PREVIOUS_AIM.
		AIM
	};

	class Input {
	public:
		double previousAim = 0.;
		double idleOffset = 0.;
		double arcMiddle = 180.;
	};

	static const script::Interface &GetInterface();


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool IsValid() const;

	// Run the script, giving the aim command for this step. Returns false if
	// the script fails, in which case the turret should keep its previous aim.
	bool Run(const Input &input, double &aim) const;


private:
	script::Program program;
};
