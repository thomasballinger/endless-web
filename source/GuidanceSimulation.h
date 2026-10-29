/* GuidanceSimulation.h
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

#include "GuidanceDrill.h"
#include "Point.h"
#include "Projectile.h"
#include "script/Script.h"
#include "Ship.h"
#include "Weapon.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

class MissileGuidance;
class Outfit;



// A small, repeatable world for trying out guidance scripts: a launcher at
// the origin fires a homing weapon at a target flying one of the guidance
// drills, every so often. The projectiles are the game's own, so the seeker
// and the script behave as in a battle. Everything starts from the same
// random seed, so replaying to a step after changing the script shows exactly
// what the change did.
class GuidanceSimulation {
public:
	// One projectile, from when it is fired until a while after it is gone.
	class Missile {
	public:
		Missile(int id, int launchStep, Projectile &&projectile);

		int id = 0;
		int launchStep = 0;
		Projectile projectile;
		// Where it has been, oldest first.
		std::deque<Point> trail;
		bool hit = false;
		// The step it hit or expired on, or -1 while it is in flight.
		int endStep = -1;
	};


public:
	// Fire the given outfit's weapon (which must be homing) at the given ship.
	GuidanceSimulation(const Outfit &outfit, const Ship &targetModel);
	GuidanceSimulation(const GuidanceSimulation &) = delete;
	GuidanceSimulation &operator=(const GuidanceSimulation &) = delete;

	// Use this guidance script (null for the weapon's own). Takes effect for
	// projectiles fired after the next Reset().
	void SetGuidance(const MissileGuidance *guidance);
	const Weapon &GetWeapon() const;

	const std::vector<GuidanceDrill> &Drills() const;
	int Drill() const;
	// Choose a drill, and start over.
	void SetDrill(int index);

	// Go back to the start, before anything has been fired.
	void Reset();
	// Advance one step: the target moves, a projectile may be fired, and
	// every projectile moves.
	void Step();
	// Start over and run to the given step.
	void ReplayTo(int step);
	int CurrentStep() const;

	const Ship &Launcher() const;
	const Ship &Target() const;
	const std::vector<Missile> &Missiles() const;
	int Launches() const;
	int Hits() const;

	// Choose which projectile to trace (-1 for none).
	void Select(int id);
	int Selected() const;
	// The selected projectile, if it is still in the simulation.
	const Missile *SelectedMissile() const;
	// How its guidance script ran on the last step, and what it was given.
	const script::Trace &SelectedTrace() const;
	// The projectile nearest the given position, within the given distance,
	// or -1 if there is none.
	int MissileAt(const Point &position, double distance) const;


private:
	void Fire();


private:
	Weapon weapon;
	const Ship *targetModel = nullptr;
	std::vector<GuidanceDrill> drills;
	int drill = 0;

	Ship launcher;
	std::shared_ptr<Ship> target;
	std::vector<Missile> missiles;
	int step = 0;
	int nextId = 0;
	int hits = 0;

	int selected = -1;
	script::Trace selectedTrace;
};
