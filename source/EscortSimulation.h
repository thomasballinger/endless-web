/* EscortSimulation.h
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

#include "EscortRole.h"
#include "GuidanceDrill.h"
#include "Point.h"
#include "Projectile.h"
#include "Random.h"
#include "script/Script.h"
#include "Ship.h"
#include "ShipManeuver.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>



class AIWorld;



// A small, repeatable world for trying out how escorts fly: a leader flies a
// drill (or is flown by the player), and an escort keeps station on it with
// a maneuver script. Given a role (see EscortRole), the escort does what the
// role says instead, and raiders come to attack, flown by the game's own AI
// (which sees the leader and the escorts as the player's ships). If the
// escort is a fighter and the leader can carry it, the leader is a carrier
// with every bay of that kind filled, so a whole squadron plays the role,
// each fighter with its own memory, and as many raiders come as there are
// fighters. Ships fire and take damage as in the game; a raider is beaten
// once it is disabled, and when a whole raid is beaten another comes later
// from somewhere else. Every ship moves in a small piece of the game (see
// AIWorld), exactly as ships move in the game. The simulation has random
// numbers of its own, so replaying to a step after the script changes shows
// exactly what the change did.
class EscortSimulation {
public:
	class Drill {
	public:
		std::string name;
		std::function<GuidanceDrill::Control(const GuidanceDrill::Situation &situation)> pilot;
	};
	// One of the escorts (a fighter, if they are carried): its ship, where it
	// has been, and how its role ran on the latest step.
	class Member {
	public:
		std::shared_ptr<Ship> ship;
		GuidanceDrill::Motion motion;
		std::deque<Point> trail;
		script::Memory roleMemory;
		script::Trace roleTrace;
		EscortRole::Output roleOutput;
		bool roleRan = false;
		bool attacking = false;
		bool returning = false;
		double lastTurn = 0.;
		Point station;
	};
	// One of the raiders.
	class RaiderShip {
	public:
		std::shared_ptr<Ship> ship;
		std::deque<Point> trail;
		bool beaten = false;
	};


public:
	EscortSimulation(const Ship &leaderModel, const Ship &escortModel);
	~EscortSimulation();

	// Fly the escort with this maneuver (or, if null, the game's).
	void SetProgram(const ShipManeuver *program);
	// Have the escorts play this role (or, if null, only keep station, with no
	// raiders). Takes effect from the next Reset().
	void SetRole(const EscortRole *role);

	const std::vector<Drill> &Drills() const;
	int DrillIndex() const;
	// Choose a drill, and start over.
	void SetDrill(int index);

	// Go back to the start, and advance one step, or to the given step.
	void Reset();
	void Step();
	void ReplayTo(int step);
	int CurrentStep() const;
	// What every escort decided on each step so far (a summary of each step's
	// decisions). Two runs are the same up to the first step on which these
	// differ, since nothing else in the simulation can differ.
	const std::vector<uint64_t> &Decisions() const;
	// Start over and replay up to the given step, but stop at the first step
	// on which the escorts decide something other than the given decisions
	// say (from another run). Returns that step, or -1 if there was none.
	int ReplayUntilDifferent(const std::vector<uint64_t> &before, int toStep);

	// Fly the leader with this control from the current step on (recorded, so
	// that replays fly it the same way), or forget how it was flown.
	void SetLeaderControl(const GuidanceDrill::Control &control);
	void ClearLeaderControls();

	// The leader, and the escorts. One of the escorts is followed: its role's
	// run is the one shown, and Escort() is it.
	const Ship &Leader() const;
	const std::deque<Point> &LeaderTrail() const;
	const std::vector<Member> &Members() const;
	int Followed() const;
	void Follow(int index);
	const Ship &Escort() const;
	const std::deque<Point> &EscortTrail() const;

	// How the escort's maneuver ran on the latest step: what it was given and
	// decided, and whether it ran at all.
	const script::Trace &LastTrace() const;
	const ShipManeuver::Input &LastInput() const;
	const ShipManeuver::Output &LastOutput() const;
	bool LastRan() const;

	// The raiders (those of the current raid, even once they are beaten), the
	// one that is the threat now (the nearest that is not beaten), and how
	// many have been beaten (disabled or destroyed).
	const std::vector<RaiderShip> &Raiders() const;
	bool HasRaider() const;
	const Ship &Raider() const;
	int RaidersDestroyed() const;
	// How many escorts have been destroyed, and the step on which you (or all
	// the escorts) were destroyed, or -1.
	int EscortsLost() const;
	int DestroyedStep() const;
	// Whether the escorts are fighters carried by the leader; if so, whether
	// the followed one is out of its bay, and whether it is on its way back.
	bool IsCarried() const;
	bool EscortIsOut() const;
	bool EscortIsReturning() const;
	int EscortsOut() const;
	const std::vector<Projectile> &Shots() const;
	// How the followed escort's role ran on the latest step, what it decided,
	// and where its station is (if it is keeping station).
	const script::Trace &RoleTrace() const;
	bool RoleRan() const;
	const EscortRole::Output &RoleOutput() const;
	const Point &StationPoint() const;

	// How far the escort is from the leader now, on average so far, and the
	// fraction of steps so far that it was within the given distance.
	double Distance() const;
	double MeanDistance() const;
	double FractionWithin(double distance) const;


private:
	// An escort keeps station at a point.
	void KeepStationAt(Member &member, const Point &point);
	// The escort does what its role says.
	void PlayRole(Member &member);
	// A raid arrives, if one is due.
	void ArriveRaid();
	// The nearest raider to the leader that can still fight, if any.
	std::shared_ptr<Ship> Threat() const;
	EscortRole::Input RoleInput(const Member &member, const std::shared_ptr<Ship> &threat) const;
	bool IsOut(const Member &member) const;


private:
	const ShipManeuver *program = nullptr;
	std::vector<Drill> drills;
	int drill = 0;
	int step = 0;
	// The models the ships are made from, and the ships themselves, in the
	// world they move in (made anew on each Reset()).
	Ship leaderModel;
	Ship escortModel;
	std::shared_ptr<Ship> leader;
	std::vector<Member> members;
	int followed = 0;
	std::unique_ptr<AIWorld> world;
	Random::Saved random;
	GuidanceDrill::Motion leaderMotion;
	std::vector<GuidanceDrill::Control> leaderControls;
	std::deque<Point> leaderTrail;
	std::vector<double> distances;
	std::vector<uint64_t> decisions;
	script::Trace trace;
	ShipManeuver::Input input;
	ShipManeuver::Output output;
	bool ran = false;

	const EscortRole *role = nullptr;
	// The raiders: pirates in ships just like the escorts' (so that the fight
	// is a fair one), flown by the game's AI.
	std::vector<RaiderShip> raiders;
	int raidReturns = 0;
	int raids = 0;
	int destroyed = 0;
	int destroyedStep = -1;
	bool carried = false;
};
