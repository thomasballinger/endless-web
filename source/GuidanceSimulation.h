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

class AIWorld;
class DefenseDaemon;
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
			enum class Kind { KEPT, LOST, REGAINED, MISSED, CONFUSED, HACKED };
			int step = 0;
			Point position;
			Kind kind = Kind::KEPT;
			double chance = 0.;
			std::string reason;
		};
		std::vector<LockEvent> lockEvents;
		bool hit = false;
		// Whether the target's anti-missile shot it down.
		bool shotDown = false;
		// The step it hit, was shot down, or expired on, or -1 while it is in
		// flight.
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
	~GuidanceSimulation();
	GuidanceSimulation(const GuidanceSimulation &) = delete;
	GuidanceSimulation &operator=(const GuidanceSimulation &) = delete;

	// Use this guidance script (null for the weapon's own). Takes effect for
	// projectiles fired after the next Reset().
	void SetGuidance(const MissileGuidance *guidance);
	const Weapon &GetWeapon() const;
	// Have the target run a defense daemon (or none, with null): it hacks the
	// missiles chasing it, as a ship with a code injector would. Takes
	// effect from the next Reset().
	void SetTargetDaemon(const DefenseDaemon *daemon);
	const DefenseDaemon *TargetDaemon() const;

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
	// The same for the target: from the step it is first flown, it flies as
	// recorded instead of flying its drill. Changing the drill forgets it.
	void SetTargetControl(const GuidanceDrill::Control &control);
	void ClearTargetControls();
	bool HasTargetControls() const;
	int CurrentStep() const;

	// The ships, which take damage as ships do in the game: the target from
	// the missiles, and the launcher from the target, if it fights back.
	const Ship &Launcher() const;
	const Ship &Target() const;
	const std::vector<Missile> &Missiles() const;
	// The shots the ships themselves fired (the target's, at the launcher),
	// still flying.
	const std::vector<Projectile> &Shots() const;
	int Launches() const;
	int Hits() const;
	// The step on which the target or the launcher was destroyed, or -1.
	int DestroyedStep() const;

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
	// How the target was flown, from the step it was first flown on.
	std::vector<GuidanceDrill::Control> targetControls;
	int targetControlStart = -1;
	// How the target and launcher are moving, when they are flown.
	GuidanceDrill::Motion targetMotion;
	GuidanceDrill::Motion launcherMotion;
	std::vector<Missile> missiles;
	int step = 0;
	int nextId = 0;
	int hits = 0;
	int destroyedStep = -1;

	// The simulation's own random numbers (see Random::Own).
	Random::Saved random;
	// How each projectile's guidance script ran, by projectile.
	std::map<int, std::vector<MissileGuidance::Record>> records;

	// How each projectile's script last ran, by projectile. (Each projectile
	// keeps a pointer to its own, so these are kept in a map, not a vector.)
	std::map<int, script::Trace> traces;
	int selected = -1;

	// For a drill flown by the game's AI: its world, and the launcher's stand-in
	// there (which the AI sees as the player's ship).
	std::unique_ptr<AIWorld> world;
	std::shared_ptr<Ship> launcherShip;

	// The target's defense daemon, if it runs one, and how long until its
	// injector can hack again.
	const DefenseDaemon *daemon = nullptr;
	int daemonCooldown = 0;
};
