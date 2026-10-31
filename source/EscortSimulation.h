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

#include "GuidanceDrill.h"
#include "Point.h"
#include "script/Script.h"
#include "Ship.h"
#include "ShipManeuver.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>



// A small, repeatable world for trying out the "keep station" maneuver that
// escorts fly: a leader flies a drill (or is flown by the player), and an
// escort keeps station on it with a maneuver script. Ships move as the game
// moves them: turning at their turn rate, thrusting forward or in reverse,
// and slowed by drag. Nothing is random, so replaying to a step after the
// script changes shows exactly what the change did.
class EscortSimulation {
public:
	class Drill {
	public:
		std::string name;
		std::function<GuidanceDrill::Control(const GuidanceDrill::Situation &situation)> pilot;
	};


public:
	EscortSimulation(const Ship &leaderModel, const Ship &escortModel);

	// Fly the escort with this maneuver (or, if null, the game's).
	void SetProgram(const ShipManeuver *program);

	const std::vector<Drill> &Drills() const;
	int DrillIndex() const;
	// Choose a drill, and start over.
	void SetDrill(int index);

	// Go back to the start, and advance one step, or to the given step.
	void Reset();
	void Step();
	void ReplayTo(int step);
	int CurrentStep() const;

	// Fly the leader with this control from the current step on (recorded, so
	// that replays fly it the same way), or forget how it was flown.
	void SetLeaderControl(const GuidanceDrill::Control &control);
	void ClearLeaderControls();

	// The ships, where they are now (for drawing them).
	const Ship &Leader() const;
	const Ship &Escort() const;
	// Where each has been, oldest first.
	const std::deque<Point> &LeaderTrail() const;
	const std::deque<Point> &EscortTrail() const;

	// How the escort's maneuver ran on the latest step: what it was given and
	// decided, and whether it ran at all.
	const script::Trace &LastTrace() const;
	const ShipManeuver::Input &LastInput() const;
	const ShipManeuver::Output &LastOutput() const;
	bool LastRan() const;

	// How far the escort is from the leader now, on average so far, and the
	// fraction of steps so far that it was within the given distance.
	double Distance() const;
	double MeanDistance() const;
	double FractionWithin(double distance) const;


private:
	void Fly(GuidanceDrill::Motion &motion, const Ship &ship, double turn, bool forward, bool back);


private:
	const ShipManeuver *program = nullptr;
	std::vector<Drill> drills;
	int drill = 0;
	int step = 0;
	Ship leader;
	Ship escort;
	GuidanceDrill::Motion leaderMotion;
	GuidanceDrill::Motion escortMotion;
	std::vector<GuidanceDrill::Control> leaderControls;
	std::deque<Point> leaderTrail;
	std::deque<Point> escortTrail;
	std::vector<double> distances;
	double lastTurn = 0.;
	script::Trace trace;
	ShipManeuver::Input input;
	ShipManeuver::Output output;
	bool ran = false;
};
