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
#include "MissileGuidance.h"
#include "Point.h"
#include "Projectile.h"
#include "Random.h"
#include "script/Script.h"
#include "Ship.h"
#include "Weapon.h"

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
		// Where it has been, oldest first, and whether its seeker was confused
		// (and steering instead of the script) at each of those points.
		std::deque<Point> trail;
		std::deque<bool> confused;
		// And whether its seeker had a lock.
		std::deque<bool> locked;
		// Each time its seeker checked its lock (and what came of it), or got
		// confused: where, with what odds, and why.
		class LockEvent {
		public:
			enum class Kind { KEPT, LOST, REGAINED, MISSED, CONFUSED };
			int step = 0;
			Point position;
			Kind kind = Kind::KEPT;
			double chance = 0.;
			std::string reason;
		};
		std::vector<LockEvent> lockEvents;
		bool hit = false;
		// The step it hit or expired on, or -1 while it is in flight.
		int endStep = -1;
		// The last step its seeker lost its lock on, or -1.
		int lostLockStep = -1;
	};


	// Where another script would first have steered differently.
	class Difference {
	public:
		// The step, or -1 if it would have done the same so far.
		int step = -1;
		// Which projectile, and where it was when its script ran.
		int missile = -1;
		Point position;
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

	// Fly the launcher with this control from the current step on, until
	// told otherwise. Each step's control is recorded, so replaying (to a
	// step after an edit, or to go back) flies it the same way; stepping on
	// past what was recorded repeats the last control given. Clear() forgets
	// the recording, leaving the launcher where it starts.
	void SetLauncherControl(const GuidanceDrill::Control &control);
	void ClearLauncherControls();
	bool HasLauncherControls() const;
	int CurrentStep() const;

	const Ship &Launcher() const;
	const Ship &Target() const;
	const std::vector<Missile> &Missiles() const;
	int Launches() const;
	int Hits() const;

	// The first step on which the given script would have steered any
	// projectile differently than the one in use did. Nothing that happened
	// before that step would change if the given script were used instead.
	// (The script is run on what each projectile's script was given; nothing
	// is simulated.)
	Difference FirstDifference(const MissileGuidance &guidance) const;

	// Choose a projectile to follow (-1 for none).
	void Select(int id);
	int Selected() const;
	// The projectile being followed: the selected one (even after it hits or
	// expires, as long as the simulation still has it), or else the newest one
	// in flight (or null, if none is).
	const Missile *Followed() const;
	// How a projectile's guidance script ran on the last step it ran.
	const script::Trace &TraceOf(int id) const;
	// What its script was given and decided the last time it ran (or null if
	// it hasn't run yet).
	const MissileGuidance::Record *LastRecord(int id) const;
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
	// How the launcher was flown, by step.
	std::vector<GuidanceDrill::Control> launcherControls;
	// How the target and launcher are moving, when they are flown.
	GuidanceDrill::Motion targetMotion;
	GuidanceDrill::Motion launcherMotion;
	std::vector<Missile> missiles;
	int step = 0;
	int nextId = 0;
	int hits = 0;

	// The simulation's own random numbers (see OwnRandom).
	Random::Saved random;
	// How each projectile's guidance script ran, by projectile.
	std::map<int, std::vector<MissileGuidance::Record>> records;

	// How each projectile's script last ran, by projectile. (Each projectile
	// keeps a pointer to its own, so these are kept in a map, not a vector.)
	std::map<int, script::Trace> traces;
	int selected = -1;
};
