/* EscortSimulation.cpp
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

#include "EscortSimulation.h"

#include "Angle.h"
#include "GameData.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	using Control = GuidanceDrill::Control;
	using Situation = GuidanceDrill::Situation;

	// How many recent positions to keep for each ship's trail.
	const size_t TRAIL_LENGTH = 600;

	// Turn toward a heading, as fast as the ship can but without overshooting,
	// thrusting once it is facing roughly that way.
	Control Head(const Situation &situation, const Angle &heading, double thrust = 1.)
	{
		const double off = (heading - situation.ship.facing).Degrees();
		Control control;
		control.turn = max(-1., min(1., off / max(.001, situation.turnRate)));
		control.thrust = fabs(off) < 50. ? thrust : 0.;
		return control;
	}

	vector<EscortSimulation::Drill> MakeDrills()
	{
		vector<EscortSimulation::Drill> drills;
		drills.push_back({"cruising", [](const Situation &situation) {
			// Orbit a point ahead, wandering in and out.
			const Point around(0., -900.);
			const double radius = 600. + 250. * sin(situation.step / 300.);
			const Point offset = situation.ship.position - around;
			const Angle bearing = offset ? Angle(offset) : Angle();
			const Point aim = around + (bearing - Angle(55.)).Unit() * radius;
			return Head(situation, Angle(aim - situation.ship.position)); }});
		drills.push_back({"zigzag", [](const Situation &situation) {
			// Full speed, swinging 70 degrees one way and then the other.
			return Head(situation, Angle(sin(situation.step / 150.) > 0. ? 70. : -70.)); }});
		drills.push_back({"sprint, then stop", [](const Situation &situation) {
			// Off at full speed, then turn around to stop, then hold still.
			if(situation.step < 300)
				return Head(situation, Angle(0.));
			if(situation.ship.velocity.Length() > .2)
				return Head(situation, Angle(-situation.ship.velocity));
			return Control(); }});
		drills.push_back({"holding still", [](const Situation &) {
			return Control(); }});
		return drills;
	}
}



EscortSimulation::EscortSimulation(const Ship &leaderModel, const Ship &escortModel)
	: drills(MakeDrills()), leader(leaderModel), escort(escortModel)
{
	Reset();
}



void EscortSimulation::SetProgram(const ShipManeuver *program)
{
	this->program = program;
}



const vector<EscortSimulation::Drill> &EscortSimulation::Drills() const
{
	return drills;
}



int EscortSimulation::DrillIndex() const
{
	return drill;
}



void EscortSimulation::SetDrill(int index)
{
	if(index < 0 || index >= static_cast<int>(drills.size()))
		return;
	drill = index;
	Reset();
}



void EscortSimulation::Reset()
{
	step = 0;
	leaderMotion = GuidanceDrill::Motion{Point(), Point(), Angle()};
	escortMotion = GuidanceDrill::Motion{Point(250., 300.), Point(), Angle()};
	leader.SetKinematics(leaderMotion.position, leaderMotion.velocity, leaderMotion.facing);
	escort.SetKinematics(escortMotion.position, escortMotion.velocity, escortMotion.facing);
	leaderTrail.clear();
	escortTrail.clear();
	distances.clear();
	lastTurn = 0.;
	trace.Clear();
	input = ShipManeuver::Input();
	output = ShipManeuver::Output();
	ran = false;
}



void EscortSimulation::Step()
{
	++step;
	// The leader moves first: flown as recorded, if it was, or by its pilot.
	GuidanceDrill::Control control;
	if(!leaderControls.empty())
		control = leaderControls[min(static_cast<size_t>(step - 1), leaderControls.size() - 1)];
	else
	{
		GuidanceDrill::Situation situation;
		situation.step = step;
		situation.ship = leaderMotion;
		situation.turnRate = leader.TurnRate();
		situation.acceleration = leader.Acceleration();
		situation.launcher = escortMotion.position;
		control = drills[drill].pilot(situation);
	}
	GuidanceDrill::Fly(leaderMotion, control, leader.TurnRate(), leader.Acceleration(), leader.MaxVelocity());
	leader.SetKinematics(leaderMotion.position, leaderMotion.velocity, leaderMotion.facing);

	// Then the escort keeps station on it, as the AI has it do.
	input = ShipManeuver::Input();
	input.position = escortMotion.position;
	input.velocity = escortMotion.velocity;
	input.facing = escortMotion.facing.Unit();
	input.facingDegrees = escortMotion.facing.Degrees();
	input.maxVelocity = escort.MaxVelocity();
	input.acceleration = escort.Acceleration();
	input.turnRate = escort.TurnRate();
	input.inertialMass = escort.InertialMass();
	input.dragForce = escort.DragForce();
	input.reverseThrust = escort.ReverseThrust();
	input.lastTurn = lastTurn;
	input.topSpeed = input.maxVelocity;
	input.targetPosition = leaderMotion.position;
	input.targetVelocity = leaderMotion.velocity;
	input.targetFacing = leaderMotion.facing.Unit();
	const ShipManeuver *maneuver = program ? program : GameData::Maneuvers().Find("keep station");
	output = ShipManeuver::Output();
	trace.Clear();
	ran = maneuver && maneuver->Run(input, output, nullptr, &trace);
	if(!ran)
		output = ShipManeuver::Output();
	const double turn = max(-1., min(1., output.turn));
	lastTurn = turn;
	Fly(escortMotion, escort, turn, output.forward, output.back);
	escort.SetKinematics(escortMotion.position, escortMotion.velocity, escortMotion.facing);

	leaderTrail.push_back(leaderMotion.position);
	escortTrail.push_back(escortMotion.position);
	if(leaderTrail.size() > TRAIL_LENGTH)
	{
		leaderTrail.pop_front();
		escortTrail.pop_front();
	}
	distances.push_back(escortMotion.position.Distance(leaderMotion.position));
}



void EscortSimulation::ReplayTo(int targetStep)
{
	Reset();
	while(step < targetStep)
		Step();
}



int EscortSimulation::CurrentStep() const
{
	return step;
}



void EscortSimulation::SetLeaderControl(const GuidanceDrill::Control &control)
{
	// What happens from the next step on is no longer what was recorded.
	leaderControls.resize(step, leaderControls.empty() ? GuidanceDrill::Control() : leaderControls.back());
	leaderControls.push_back(control);
}



void EscortSimulation::ClearLeaderControls()
{
	leaderControls.clear();
}



const Ship &EscortSimulation::Leader() const
{
	return leader;
}



const Ship &EscortSimulation::Escort() const
{
	return escort;
}



const deque<Point> &EscortSimulation::LeaderTrail() const
{
	return leaderTrail;
}



const deque<Point> &EscortSimulation::EscortTrail() const
{
	return escortTrail;
}



const script::Trace &EscortSimulation::LastTrace() const
{
	return trace;
}



const ShipManeuver::Input &EscortSimulation::LastInput() const
{
	return input;
}



const ShipManeuver::Output &EscortSimulation::LastOutput() const
{
	return output;
}



bool EscortSimulation::LastRan() const
{
	return ran;
}



double EscortSimulation::Distance() const
{
	return distances.empty() ? escortMotion.position.Distance(leaderMotion.position) : distances.back();
}



double EscortSimulation::MeanDistance() const
{
	if(distances.empty())
		return Distance();
	double total = 0.;
	for(double distance : distances)
		total += distance;
	return total / distances.size();
}



double EscortSimulation::FractionWithin(double distance) const
{
	if(distances.empty())
		return 0.;
	return static_cast<double>(count_if(distances.begin(), distances.end(),
		[distance](double d) { return d <= distance; })) / distances.size();
}



void EscortSimulation::Fly(GuidanceDrill::Motion &motion, const Ship &ship, double turn, bool forward, bool back)
{
	// As a ship moves: turn, thrust forward or in reverse, then drag.
	motion.facing += Angle(turn * ship.TurnRate());
	if(back && ship.InertialMass() > 0.)
		motion.velocity -= motion.facing.Unit() * (ship.ReverseThrust() / ship.InertialMass());
	else if(forward)
		motion.velocity += motion.facing.Unit() * ship.Acceleration();
	motion.velocity *= 1. - ship.DragForce();
	motion.position += motion.velocity;
}
