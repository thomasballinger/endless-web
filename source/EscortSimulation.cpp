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
#include "ship/ShipAICache.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace std;

namespace {
	using Control = GuidanceDrill::Control;
	using Situation = GuidanceDrill::Situation;

	// How many recent positions to keep for each ship's trail.
	const size_t TRAIL_LENGTH = 600;
	// When the first raid arrives, how far from the leader, how far apart its
	// raiders are, and how long until another comes after one is beaten.
	const int RAIDER_ARRIVES = 240;
	const double RAIDER_DISTANCE = 2000.;
	const double RAIDER_SPACING = 160.;
	const int RAIDER_RETURNS = 300;
	// What raids a squadron of fighters.
	const string FIGHTER_RAIDERS = "Marauder Leviathan";
	const int FIGHTER_RAID_SIZE = 2;
	// The random seed everything starts from.
	const uint64_t SEED = 1;

	// Mix a number into a hash of a step's decisions.
	void Mix(uint64_t &hash, uint64_t value)
	{
		hash = (hash ^ value) * 1099511628211ull;
	}
	void Mix(uint64_t &hash, double value)
	{
		uint64_t bits;
		memcpy(&bits, &value, sizeof(bits));
		Mix(hash, bits);
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
	// The leader and the escorts are the player's ships, in a world of their
	// own (a fresh one each time, so replays are the same), flown by the
	// simulator's commands.
	world = make_unique<AIWorld>();
	leader = make_shared<Ship>(leaderModel);
	leader->SetKinematics(leaderMotion.position, leaderMotion.velocity, leaderMotion.facing);
	leader->SetGovernment(GameData::PlayerGovernment());
	world->AddPlayerShip(leader);
	members.clear();
	// Fighters start in their carrier's bays, ordered to deploy (as if you had
	// pressed the deploy key): as many as it has bays for them, when they play
	// a role. Otherwise there is one escort.
	auto escort = make_shared<Ship>(escortModel);
	escort->SetKinematics(Point(250., 300.), Point(), Angle());
	carried = world->AddCarried(leader, escort);
	if(carried)
	{
		members.emplace_back();
		members.back().ship = escort;
		while(role)
		{
			auto another = make_shared<Ship>(escortModel);
			if(!world->AddCarried(leader, another))
				break;
			members.emplace_back();
			members.back().ship = another;
		}
		for(Member &member : members)
		{
			member.ship->SetDeployOrder(true);
			member.motion = leaderMotion;
		}
	}
	else
	{
		escort->SetGovernment(GameData::PlayerGovernment());
		world->AddPlayerShip(escort);
		members.emplace_back();
		members.back().ship = escort;
		members.back().motion = GuidanceDrill::Motion{Point(250., 300.), Point(), Angle()};
	}
	followed = min(followed, static_cast<int>(members.size()) - 1);
	leaderTrail.clear();
	distances.clear();
	decisions.clear();
	trace.Clear();
	input = ShipManeuver::Input();
	output = ShipManeuver::Output();
	ran = false;

	raiders.clear();
	raidReturns = RAIDER_ARRIVES;
	raids = 0;
	destroyed = 0;
	destroyedStep = -1;
}



void EscortSimulation::Step()
{
	Random::Own ownRandom(random);
	++step;
	// A raid arrives, if one is due. Then the game's AI decides what every
	// ship does, seeing every ship where it is now.
	if(role)
		ArriveRaid();
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
		situation.launcher = members[0].motion.position;
		control = drills[drill].pilot(situation);
	}
	// (A carrier with fighters ready to go also deploys them, as the AI said.)
	Command leaderCommand = AIWorld::ToCommand(control);
	if(leader->Commands().Has(Command::DEPLOY))
		leaderCommand |= Command::DEPLOY;
	leader->SetCommands(leaderCommand);

	// Each escort does what its role says, or keeps station.
	uint64_t decided = 1469598103934665603ull;
	for(Member &member : members)
	{
		member.attacking = false;
		member.returning = false;
		if(member.ship->IsDestroyed())
			continue;
		// A fighter in its bay does nothing, and one the AI is bringing back to
		// its bay (recalled, or badly damaged) does that first, as in the game.
		member.returning = carried && IsOut(member) && member.ship->Commands().Has(Command::BOARD);
		if(!IsOut(member) || member.returning)
		{
			member.station = leaderMotion.position;
			member.lastTurn = member.ship->Commands().Turn();
		}
		else if(role)
			PlayRole(member);
		else
			KeepStationAt(member, leaderMotion.position);
		Mix(decided, static_cast<uint64_t>(member.returning) + 2 * IsOut(member));
		Mix(decided, static_cast<uint64_t>(member.roleRan) + 2 * member.roleOutput.chose
			+ 4 * static_cast<uint64_t>(member.roleOutput.behavior) + 64 * member.attacking);
		Mix(decided, member.roleOutput.station.X());
		Mix(decided, member.roleOutput.station.Y());
		if(!role)
		{
			Mix(decided, output.turn);
			Mix(decided, static_cast<uint64_t>(output.forward) + 2 * output.back + 4 * output.afterburner);
		}
	}
	decisions.push_back(decided);

	// Then every ship moves, as ships move in the game.
	world->Move();
	leaderMotion = GuidanceDrill::Motion{leader->Position(), leader->Velocity(), leader->Facing()};
	// (A fighter in its bay is where its carrier is.)
	for(Member &member : members)
	{
		const Ship &ship = *member.ship;
		member.motion = IsOut(member) ? GuidanceDrill::Motion{ship.Position(), ship.Velocity(), ship.Facing()}
			: leaderMotion;
	}

	// The ships fought, as ships fight in the game. A raider is beaten once it
	// is disabled; it stays until it is destroyed, or the next raid comes.
	bool raidBeaten = !raiders.empty();
	for(RaiderShip &raider : raiders)
	{
		if(!raider.beaten && raider.ship->IsDisabled())
		{
			raider.beaten = true;
			++destroyed;
		}
		raidBeaten &= raider.beaten;
	}
	// (Until it is beaten, the next raid isn't due.)
	if(raidBeaten && raidReturns == numeric_limits<int>::max())
		raidReturns = step + RAIDER_RETURNS;
	int alive = 0;
	for(const Member &member : members)
		alive += !member.ship->IsDestroyed();
	if(destroyedStep < 0 && (leader->IsDestroyed() || !alive))
		destroyedStep = step;

	auto record = [](deque<Point> &trail, const Point &point) {
		trail.push_back(point);
		if(trail.size() > TRAIL_LENGTH)
			trail.pop_front();
	};
	record(leaderTrail, leaderMotion.position);
	for(Member &member : members)
		if(!member.ship->ShouldBeRemoved() || !member.ship->IsDestroyed())
			record(member.trail, member.motion.position);
	for(RaiderShip &raider : raiders)
		if(!raider.ship->ShouldBeRemoved())
			record(raider.trail, raider.ship->Position());
	distances.push_back(members[0].motion.position.Distance(leaderMotion.position));
}



void EscortSimulation::PlayRole(Member &member)
{
	const shared_ptr<Ship> threat = Threat();
	member.roleTrace.Clear();
	member.roleRan = role->Run(RoleInput(member, threat), member.roleOutput, &member.roleMemory, &member.roleTrace);
	const EscortRole::Effect behavior = (member.roleRan && member.roleOutput.chose) ? member.roleOutput.behavior
		: EscortRole::INDEPENDENT;
	// As in the game (see AI::RunRole): an attack goes after the threat, your
	// target, or the escort's own, if that can be fought; the game's AI flies
	// it in.
	auto usable = [](const shared_ptr<Ship> &ship) {
		return ship && ship->GetSystem() && ship->IsTargetable() && !ship->IsDisabled();
	};
	shared_ptr<Ship> victim;
	if(behavior == EscortRole::ATTACK_THREAT)
		victim = threat;
	else if(behavior == EscortRole::ATTACK_LEADER_TARGET)
		victim = leader->GetTargetShip();
	else if(behavior == EscortRole::ATTACK_TARGET)
		victim = member.ship->GetTargetShip();
	if(usable(victim))
	{
		member.attacking = true;
		member.ship->SetTargetShip(victim);
		world->FlyIndependent(member.ship);
		member.lastTurn = member.ship->Commands().Turn();
		member.station = victim->Position();
	}
	else if(behavior == EscortRole::KEEP_STATION || behavior == EscortRole::RETREAT)
	{
		// The station is relative to the leader: x to its right, y ahead.
		const Point station = (behavior == EscortRole::RETREAT) ? Point(0., -600.) : member.roleOutput.station;
		const Point forward = leaderMotion.facing.Unit();
		const Point right(-forward.Y(), forward.X());
		KeepStationAt(member, leaderMotion.position + right * station.X() + forward * station.Y());
	}
	else
	{
		// Left to the usual AI (or an attack with nothing to attack), the
		// escort flies as the AI already decided.
		member.lastTurn = member.ship->Commands().Turn();
		member.station = leaderMotion.position;
	}
}



void EscortSimulation::KeepStationAt(Member &member, const Point &point)
{
	const Ship &escort = *member.ship;
	member.station = point;
	input = ShipManeuver::Input();
	input.position = member.motion.position;
	input.velocity = member.motion.velocity;
	input.facing = member.motion.facing.Unit();
	input.facingDegrees = member.motion.facing.Degrees();
	input.maxVelocity = escort.MaxVelocity();
	input.acceleration = escort.Acceleration();
	input.turnRate = escort.TurnRate();
	input.inertialMass = escort.InertialMass();
	input.dragForce = escort.DragForce();
	input.reverseThrust = escort.ReverseThrust();
	input.lastTurn = member.lastTurn;
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
	member.lastTurn = turn;
	Command command;
	command.SetTurn(turn);
	if(output.back)
		command |= Command::BACK;
	else if(output.forward)
		command |= Command::FORWARD;
	if(output.afterburner)
		command |= Command::AFTERBURNER;
	member.ship->SetCommands(command);
}



void EscortSimulation::ArriveRaid()
{
	bool beaten = true;
	for(const RaiderShip &raider : raiders)
		beaten &= raider.beaten;
	if(!beaten || step < raidReturns)
		return;
	for(const RaiderShip &raider : raiders)
		world->Remove(raider.ship);
	raiders.clear();
	// Each raid comes from a different side, in a line abreast, flown by the
	// game's AI, which sees the leader and the escorts as the player's ships.
	const Angle from(110. + 137. * raids);
	const Point across = (from + Angle(90.)).Unit();
	// An escort meets pirates in the same kind of ship; a squadron of fighters
	// meets a raid that outguns it (so that, as in the game, the fighters fight
	// with all they have, rather than saving their ammunition).
	int count = members.size();
	const Ship *raiderModel = &escortModel;
	if(carried)
		if(const Ship *warship = GameData::Ships().Find(FIGHTER_RAIDERS))
		{
			raiderModel = warship;
			count = FIGHTER_RAID_SIZE;
		}
	for(int i = 0; i < count; ++i)
	{
		RaiderShip raider;
		raider.ship = make_shared<Ship>(*raiderModel);
		raider.ship->SetGovernment(GameData::Governments().Get("Pirate"));
		const Point position = leaderMotion.position + from.Unit() * RAIDER_DISTANCE
			+ across * (RAIDER_SPACING * (i - (count - 1) / 2.));
		raider.ship->SetKinematics(position, Point(), from + Angle(180.));
		world->AddShip(raider.ship, AIWorld::Pilot::AI, "heroic vindictive staying nemesis");
		raider.ship->SetTargetShip(leader);
		raiders.push_back(std::move(raider));
	}
	// You target the first of them, as a player would.
	leader->SetTargetShip(raiders.front().ship);
	raidReturns = numeric_limits<int>::max();
	++raids;
}



shared_ptr<Ship> EscortSimulation::Threat() const
{
	// As in the game: the nearest hostile ship to the leader that can fight.
	shared_ptr<Ship> threat;
	double closest = 4000.;
	for(const RaiderShip &raider : raiders)
		if(raider.ship->GetSystem() && raider.ship->IsTargetable() && !raider.ship->IsDisabled())
		{
			const double distance = raider.ship->Position().Distance(leaderMotion.position);
			if(distance < closest)
			{
				closest = distance;
				threat = raider.ship;
			}
		}
	return threat;
}



EscortRole::Input EscortSimulation::RoleInput(const Member &member, const shared_ptr<Ship> &threat) const
{
	// What a role is told in the game (see AI::RunRole).
	const Ship &escort = *member.ship;
	EscortRole::Input in;
	in.position = member.motion.position;
	in.velocity = member.motion.velocity;
	in.facing = member.motion.facing.Unit();
	in.hull = escort.HullFraction();
	in.shields = escort.ShieldFraction();
	in.energy = escort.EnergyFraction();
	in.weaponRange = escort.GetAICache().LongestRange();
	in.leaderPosition = leaderMotion.position;
	in.leaderVelocity = leaderMotion.velocity;
	in.leaderFacing = leaderMotion.facing.Unit();
	in.leaderHull = leader->HullFraction();
	in.leaderShields = leader->ShieldFraction();
	auto usable = [](const shared_ptr<Ship> &ship) {
		return ship && ship->GetSystem() && ship->IsTargetable() && !ship->IsDisabled();
	};
	const shared_ptr<Ship> leaderTarget = leader->GetTargetShip();
	if(usable(leaderTarget))
	{
		in.leaderTarget = true;
		in.leaderTargetPosition = leaderTarget->Position();
		in.leaderTargetHull = leaderTarget->HullFraction();
	}
	if(threat)
	{
		in.threat = true;
		in.threatPosition = threat->Position();
		in.threatVelocity = threat->Velocity();
		in.threatHull = threat->HullFraction();
		in.threatTargetingLeader = (threat->GetTargetShip() == leader);
	}
	const shared_ptr<Ship> target = escort.GetTargetShip();
	if(usable(target))
	{
		in.target = true;
		in.targetPosition = target->Position();
		in.targetHull = target->HullFraction();
	}
	// Its place among the escorts that are out (and not destroyed).
	in.escortCount = 0;
	for(const Member &other : members)
		if(IsOut(other) && !other.ship->IsDestroyed())
		{
			if(&other == &member)
				in.escortIndex = in.escortCount;
			++in.escortCount;
		}
	return in;
}



bool EscortSimulation::IsOut(const Member &member) const
{
	return !carried || member.ship->GetSystem();
}



void EscortSimulation::ReplayTo(int targetStep)
{
	Reset();
	while(step < targetStep)
		Step();
}



int EscortSimulation::ReplayUntilDifferent(const vector<uint64_t> &before, int toStep)
{
	Reset();
	while(step < toStep)
	{
		Step();
		const size_t index = step - 1;
		if(index >= before.size() || decisions[index] != before[index])
			return step;
	}
	return -1;
}



int EscortSimulation::CurrentStep() const
{
	return step;
}



const vector<uint64_t> &EscortSimulation::Decisions() const
{
	return decisions;
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



const deque<Point> &EscortSimulation::LeaderTrail() const
{
	return leaderTrail;
}



const vector<EscortSimulation::Member> &EscortSimulation::Members() const
{
	return members;
}



int EscortSimulation::Followed() const
{
	return followed;
}



void EscortSimulation::Follow(int index)
{
	const int count = members.size();
	followed = ((index % count) + count) % count;
}



const Ship &EscortSimulation::Escort() const
{
	return *members[followed].ship;
}



const deque<Point> &EscortSimulation::EscortTrail() const
{
	return members[followed].trail;
}



const vector<EscortSimulation::RaiderShip> &EscortSimulation::Raiders() const
{
	return raiders;
}



bool EscortSimulation::HasRaider() const
{
	for(const RaiderShip &raider : raiders)
		if(!raider.ship->ShouldBeRemoved())
			return true;
	return false;
}



const Ship &EscortSimulation::Raider() const
{
	const shared_ptr<Ship> threat = Threat();
	if(threat)
		return *threat;
	for(const RaiderShip &raider : raiders)
		if(!raider.ship->ShouldBeRemoved())
			return *raider.ship;
	return *leader;
}



int EscortSimulation::RaidersDestroyed() const
{
	return destroyed;
}



int EscortSimulation::EscortsLost() const
{
	int lost = 0;
	for(const Member &member : members)
		lost += member.ship->IsDestroyed();
	return lost;
}



int EscortSimulation::DestroyedStep() const
{
	return destroyedStep;
}



bool EscortSimulation::IsCarried() const
{
	return carried;
}



bool EscortSimulation::EscortIsOut() const
{
	return IsOut(members[followed]);
}



bool EscortSimulation::EscortIsReturning() const
{
	return members[followed].returning;
}



int EscortSimulation::EscortsOut() const
{
	int out = 0;
	for(const Member &member : members)
		out += IsOut(member) && !member.ship->IsDestroyed();
	return out;
}



const vector<Projectile> &EscortSimulation::Shots() const
{
	return world->Projectiles();
}



const script::Trace &EscortSimulation::RoleTrace() const
{
	return members[followed].roleTrace;
}



bool EscortSimulation::RoleRan() const
{
	return members[followed].roleRan;
}



const EscortRole::Output &EscortSimulation::RoleOutput() const
{
	return members[followed].roleOutput;
}



const Point &EscortSimulation::StationPoint() const
{
	return members[followed].station;
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
	return distances.empty() ? members[0].motion.position.Distance(leaderMotion.position) : distances.back();
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
