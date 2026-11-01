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

#include "AIWorld.h"
#include "Angle.h"
#include "Command.h"
#include "GameData.h"
#include "Government.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	using Control = GuidanceDrill::Control;
	using Situation = GuidanceDrill::Situation;

	// How many recent positions to keep for each ship's trail.
	const size_t TRAIL_LENGTH = 600;
	// When the first raider arrives, how far from the leader, how long until
	// another comes after one is destroyed, and the ship it flies.
	const int RAIDER_ARRIVES = 240;
	const double RAIDER_DISTANCE = 2000.;
	const int RAIDER_RETURNS = 300;
	const string RAIDER_MODEL = "Headhunter";
	// Within what range, and how close to facing it, a ship hurts another, and
	// how much each step (as a fraction of the other's hull), and how fast
	// ships recover.
	const double GUN_RANGE = 500.;
	const double GUN_ARC = 25.;
	const double RAIDER_DAMAGE = .0012;
	const double ESCORT_DAMAGE = .003;
	const double REPAIR = .0002;
	// The random seed everything starts from.
	const uint64_t SEED = 1;

	// Turn toward a direction, given in any vector, and thrust while facing
	// roughly that way.
	GuidanceDrill::Control Toward(const GuidanceDrill::Motion &motion, const Point &direction, double turnRate)
	{
		GuidanceDrill::Control control;
		if(!direction)
			return control;
		const double off = (Angle(direction) - motion.facing).Degrees();
		control.turn = max(-1., min(1., off / max(.001, turnRate)));
		control.thrust = fabs(off) < 40. ? 1. : 0.;
		return control;
	}

	// Whether one ship faces another closely enough, and is near enough, to hurt
	// it.
	bool CanHit(const GuidanceDrill::Motion &shooter, const Point &target)
	{
		const Point offset = target - shooter.position;
		return offset.Length() < GUN_RANGE && offset
			&& fabs((Angle(offset) - shooter.facing).Degrees()) < GUN_ARC;
	}

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
	: drills(MakeDrills()), leaderModel(leaderModel), escortModel(escortModel)
{
	raiderModel = GameData::Ships().Find(RAIDER_MODEL);
	Reset();
}



EscortSimulation::~EscortSimulation() = default;



void EscortSimulation::SetRole(const EscortRole *role)
{
	this->role = role;
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
	Random::Own ownRandom(random);
	Random::Seed(SEED);
	step = 0;
	leaderMotion = GuidanceDrill::Motion{Point(), Point(), Angle()};
	escortMotion = GuidanceDrill::Motion{Point(250., 300.), Point(), Angle()};
	// The leader and the escort are the player's ships, in a world of their
	// own (a fresh one each time, so replays are the same), flown by the
	// simulator's commands.
	world = make_unique<AIWorld>();
	leader = make_shared<Ship>(leaderModel);
	escort = make_shared<Ship>(escortModel);
	leader->SetKinematics(leaderMotion.position, leaderMotion.velocity, leaderMotion.facing);
	escort->SetKinematics(escortMotion.position, escortMotion.velocity, escortMotion.facing);
	for(const shared_ptr<Ship> &ship : {leader, escort})
	{
		ship->SetGovernment(GameData::PlayerGovernment());
		world->AddPlayerShip(ship);
	}
	raider = make_shared<Ship>(raiderModel ? *raiderModel : escortModel);
	leaderTrail.clear();
	escortTrail.clear();
	distances.clear();
	lastTurn = 0.;
	trace.Clear();
	input = ShipManeuver::Input();
	output = ShipManeuver::Output();
	ran = false;

	roleMemory.Clear();
	roleTrace.Clear();
	roleOutput = EscortRole::Output();
	roleRan = false;
	stationPoint = Point();
	raiderHere = false;
	raiderTrail.clear();
	raiderReturns = RAIDER_ARRIVES;
	raids = 0;
	destroyed = 0;
	leaderHull = 1.;
	escortHull = 1.;
	raiderHull = 1.;
	escortAttacking = false;
}



void EscortSimulation::Step()
{
	Random::Own ownRandom(random);
	++step;
	// A raider arrives, if one is due. Then the game's AI decides what it
	// does, seeing every ship where it is now.
	if(role && raiderModel)
		ArriveRaider();
	world->Think();

	// The leader is flown as recorded, if it was, or by its pilot.
	GuidanceDrill::Control control;
	if(!leaderControls.empty())
		control = leaderControls[min(static_cast<size_t>(step - 1), leaderControls.size() - 1)];
	else
	{
		GuidanceDrill::Situation situation;
		situation.step = step;
		situation.ship = leaderMotion;
		situation.turnRate = leader->TurnRate();
		situation.acceleration = leader->Acceleration();
		situation.launcher = escortMotion.position;
		control = drills[drill].pilot(situation);
	}
	leader->SetCommands(AIWorld::ToCommand(control));

	// The escort does what its role says, or keeps station. (The role is told
	// whether the escort was attacking on the last step.)
	if(role)
	{
		roleTrace.Clear();
		roleRan = role->Run(RoleInput(), roleOutput, &roleMemory, &roleTrace);
		const EscortRole::Effect behavior = (roleRan && roleOutput.chose) ? roleOutput.behavior
			: EscortRole::INDEPENDENT;
		const bool attacks = (behavior == EscortRole::ATTACK_THREAT || behavior == EscortRole::ATTACK_LEADER_TARGET
			|| behavior == EscortRole::ATTACK_TARGET);
		escortAttacking = attacks && raiderHere;
		if(escortAttacking)
			ChaseRaider();
		else
		{
			// The station is relative to the leader: x to its right, y ahead.
			const Point station = (behavior == EscortRole::RETREAT) ? Point(0., -600.)
				: (behavior == EscortRole::KEEP_STATION) ? roleOutput.station : Point(0., -250.);
			const Point forward = leaderMotion.facing.Unit();
			const Point right(-forward.Y(), forward.X());
			KeepStationAt(leaderMotion.position + right * station.X() + forward * station.Y());
		}
	}
	else
	{
		escortAttacking = false;
		KeepStationAt(leaderMotion.position);
	}

	// Then every ship moves, as ships move in the game.
	world->Move();
	leaderMotion = GuidanceDrill::Motion{leader->Position(), leader->Velocity(), leader->Facing()};
	escortMotion = GuidanceDrill::Motion{escort->Position(), escort->Velocity(), escort->Facing()};
	if(raiderHere)
		raiderMotion = GuidanceDrill::Motion{raider->Position(), raider->Velocity(), raider->Facing()};

	// Ships hurt what they face, and recover.
	if(raiderHere)
	{
		// The raider hurts whichever of them it is after.
		const bool onEscort = (raider->GetTargetShip() == escort);
		const Point victim = onEscort ? escortMotion.position : leaderMotion.position;
		if(CanHit(raiderMotion, victim))
		{
			if(onEscort)
				escortHull = max(0., escortHull - RAIDER_DAMAGE * 1.5);
			else
				leaderHull = max(0., leaderHull - RAIDER_DAMAGE);
		}
		if(CanHit(escortMotion, raiderMotion.position))
		{
			raiderHull -= ESCORT_DAMAGE;
			if(raiderHull <= 0.)
			{
				raiderHere = false;
				world->Remove(raider);
				raiderReturns = step + RAIDER_RETURNS;
				++destroyed;
			}
		}
	}
	leaderHull = min(1., leaderHull + REPAIR);
	escortHull = min(1., escortHull + REPAIR);

	leaderTrail.push_back(leaderMotion.position);
	escortTrail.push_back(escortMotion.position);
	if(leaderTrail.size() > TRAIL_LENGTH)
	{
		leaderTrail.pop_front();
		escortTrail.pop_front();
	}
	if(raiderHere)
	{
		raiderTrail.push_back(raiderMotion.position);
		if(raiderTrail.size() > TRAIL_LENGTH)
			raiderTrail.pop_front();
	}
	distances.push_back(escortMotion.position.Distance(leaderMotion.position));
}



void EscortSimulation::KeepStationAt(const Point &point)
{
	stationPoint = point;
	input = ShipManeuver::Input();
	input.position = escortMotion.position;
	input.velocity = escortMotion.velocity;
	input.facing = escortMotion.facing.Unit();
	input.facingDegrees = escortMotion.facing.Degrees();
	input.maxVelocity = escort->MaxVelocity();
	input.acceleration = escort->Acceleration();
	input.turnRate = escort->TurnRate();
	input.inertialMass = escort->InertialMass();
	input.dragForce = escort->DragForce();
	input.reverseThrust = escort->ReverseThrust();
	input.lastTurn = lastTurn;
	input.topSpeed = input.maxVelocity;
	input.targetPosition = point;
	input.targetVelocity = leaderMotion.velocity;
	input.targetFacing = leaderMotion.facing.Unit();
	const ShipManeuver *maneuver = program ? program : GameData::Maneuvers().Find("keep station");
	output = ShipManeuver::Output();
	trace.Clear();
	ran = maneuver && maneuver->Run(input, output, nullptr, &trace);
	if(!ran)
		output = ShipManeuver::Output();
	// The escort is given the commands the game gives it for this output.
	const double turn = max(-1., min(1., output.turn));
	lastTurn = turn;
	Command command;
	command.SetTurn(turn);
	if(output.back)
		command |= Command::BACK;
	else if(output.forward)
		command |= Command::FORWARD;
	if(output.afterburner)
		command |= Command::AFTERBURNER;
	escort->SetCommands(command);
}



void EscortSimulation::ChaseRaider()
{
	// Lead the raider while far off; close in, circle it once near.
	const Point offset = raiderMotion.position - escortMotion.position;
	const double distance = offset.Length();
	Point aim = offset + raiderMotion.velocity * (distance / max(1., escort->MaxVelocity()));
	if(distance < 250.)
		aim = offset;
	GuidanceDrill::Control control = Toward(escortMotion, aim, escort->TurnRate());
	if(distance < 200.)
		control.thrust = 0.;
	lastTurn = control.turn;
	escort->SetCommands(AIWorld::ToCommand(control));
	stationPoint = raiderMotion.position;
}



void EscortSimulation::ArriveRaider()
{
	if(raiderHere || step < raiderReturns)
		return;
	// Each raider comes from a different side: a pirate, flown by the game's
	// AI, which sees the leader and the escort as the player's ships.
	const Angle from(110. + 137. * raids);
	raider = make_shared<Ship>(*raiderModel);
	raider->SetGovernment(GameData::Governments().Get("Pirate"));
	raider->SetKinematics(leaderMotion.position + from.Unit() * RAIDER_DISTANCE, Point(), from + Angle(180.));
	world->AddShip(raider, AIWorld::Pilot::AI, "heroic staying");
	raider->SetTargetShip(leader);
	raiderMotion = GuidanceDrill::Motion{raider->Position(), raider->Velocity(), raider->Facing()};
	raiderHull = 1.;
	raiderHere = true;
	raiderTrail.clear();
	++raids;
}



EscortRole::Input EscortSimulation::RoleInput() const
{
	// What a role is told in the game (see AI::RunRole): the leader targets
	// the raider, which is the threat; the escort's own target is the raider
	// once it has attacked it.
	EscortRole::Input in;
	in.position = escortMotion.position;
	in.velocity = escortMotion.velocity;
	in.facing = escortMotion.facing.Unit();
	in.hull = escortHull;
	in.shields = escortHull;
	in.energy = 1.;
	in.weaponRange = GUN_RANGE;
	in.leaderPosition = leaderMotion.position;
	in.leaderVelocity = leaderMotion.velocity;
	in.leaderFacing = leaderMotion.facing.Unit();
	in.leaderHull = leaderHull;
	in.leaderShields = leaderHull;
	if(raiderHere)
	{
		in.leaderTarget = true;
		in.leaderTargetPosition = raiderMotion.position;
		in.leaderTargetHull = raiderHull;
		in.threat = true;
		in.threatPosition = raiderMotion.position;
		in.threatVelocity = raiderMotion.velocity;
		in.threatHull = raiderHull;
		in.threatTargetingLeader = (raider->GetTargetShip() != escort);
		in.target = escortAttacking;
		in.targetPosition = raiderMotion.position;
		in.targetHull = raiderHull;
	}
	in.escortIndex = 0;
	in.escortCount = 1;
	return in;
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
	return *leader;
}



const Ship &EscortSimulation::Escort() const
{
	return *escort;
}



const deque<Point> &EscortSimulation::LeaderTrail() const
{
	return leaderTrail;
}



const deque<Point> &EscortSimulation::EscortTrail() const
{
	return escortTrail;
}



bool EscortSimulation::HasRaider() const
{
	return raiderHere;
}



const Ship &EscortSimulation::Raider() const
{
	return *raider;
}



const deque<Point> &EscortSimulation::RaiderTrail() const
{
	return raiderTrail;
}



double EscortSimulation::LeaderHull() const
{
	return leaderHull;
}



double EscortSimulation::EscortHull() const
{
	return escortHull;
}



double EscortSimulation::RaiderHull() const
{
	return raiderHull;
}



int EscortSimulation::RaidersDestroyed() const
{
	return destroyed;
}



const script::Trace &EscortSimulation::RoleTrace() const
{
	return roleTrace;
}



bool EscortSimulation::RoleRan() const
{
	return roleRan;
}



const EscortRole::Output &EscortSimulation::RoleOutput() const
{
	return roleOutput;
}



const Point &EscortSimulation::StationPoint() const
{
	return stationPoint;
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
