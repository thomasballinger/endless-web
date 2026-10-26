/* GuidanceBench.h
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

#include <string>
#include <vector>



// A test range for missile guidance: fire a homing weapon's projectiles at a
// target ship flying along fixed paths (holding still, fleeing, crossing,
// circling, and so on), and report how often and how quickly they hit.
//
// The projectiles are the game's own, so the seeker (lock and confusion) and
// the guidance script behave exactly as in a battle. Only the target is
// moved along a set path instead of flying itself, so that runs with
// different guidance scripts can be compared directly.
class GuidanceBench {
public:
	class Options {
	public:
		// The outfit whose weapon to fire.
		std::string outfit;
		// Guidance scripts to use instead of the weapon's own. With more than
		// one, the bench compares them side by side.
		std::vector<std::string> guidance;
		// Files to load more guidance scripts from, such as a script being
		// written. Unless guidance is given, the bench compares the weapon's
		// own script with every script in these files.
		std::vector<std::string> scriptFiles;
		// The ship model to fire at.
		std::string target = "Argosy";
		// How many projectiles to fire in each scenario.
		int trials = 20;
		// If not empty, write every projectile's path to this file as JSON. With
		// several guidance scripts, each gets its own file, named by adding the
		// script's name to this one.
		std::string replayPath;
	};


public:
	// Run the bench and print the results. Returns false if the options are
	// not valid.
	static bool Run(const Options &options);
};
