/* test_maneuver.cpp
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

#include "es-test.hpp"

// Include only the tested class's header.
#include "../../../source/ShipManeuver.h"

// Include helpers for reading data files and the game code the script uses.
#include "../../../source/AI.h"
#include "../../../source/Angle.h"
#include "../../../source/DataFile.h"
#include "../../../source/DataNode.h"
#include "../../../source/pi.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>

namespace { // test namespace

// #region mock data
// Everything the built-in AI::KeepStation depended on.
struct Situation {
	Point position;
	Point velocity;
	Angle facing;
	Point targetPosition;
	Point targetVelocity;
	Angle targetFacing;
	double maxVelocity = 0.;
	double acceleration = 0.;
	double turnRate = 0.;
	double inertialMass = 0.;
	double dragForce = 0.;
	double reverseThrust = 0.;
	double lastTurn = 0.;
};

struct Steering {
	double turn = 0.;
	bool forward = false;
	bool back = false;
};

double AngleDiff(double a, double b)
{
	a = abs(a - b);
	return std::min(a, 360. - a);
}

// AI::KeepStation from before the "keep station" maneuver script replaced it,
// changed only to read from a Situation and return its commands. This is the
// reference the script must match exactly. If the game's code changes, copy
// the new version here, and update data/maneuvers.txt until this test passes.
Steering BuiltInKeepStation(const Situation &s)
{
	Steering command;
	// Constants:
	static const double MAX_TIME = 600.;
	static const double LEAD_TIME = 500.;
	static const double POSITION_DEADBAND = 200.;
	static const double VELOCITY_DEADBAND = 1.5;
	static const double TIME_DEADBAND = 120.;
	static const double THRUST_DEADBAND = .5;

	// Current properties of the two ships:
	double maxV = s.maxVelocity;
	double accel = s.acceleration;
	double turn = s.turnRate;
	double mass = s.inertialMass;
	Point unit = s.facing.Unit();
	double currentAngle = s.facing.Degrees();
	// This is where we want to be relative to where we are now:
	Point velocityDelta = s.targetVelocity - s.velocity;
	Point positionDelta = s.targetPosition + LEAD_TIME * velocityDelta - s.position;
	double positionSize = positionDelta.Length();
	double positionWeight = positionSize / (positionSize + POSITION_DEADBAND);
	// This is how fast we want to be going relative to how fast we're going now:
	velocityDelta -= unit * VELOCITY_DEADBAND;
	double velocitySize = velocityDelta.Length();
	double velocityWeight = velocitySize / (velocitySize + VELOCITY_DEADBAND);

	// Time it will take (roughly) to move to the target ship:
	double positionTime = AI::RendezvousTime(positionDelta, s.targetVelocity, maxV);
	if(std::isnan(positionTime) || positionTime > MAX_TIME)
		positionTime = MAX_TIME;
	Point rendezvous = positionDelta + s.targetVelocity * positionTime;
	double positionAngle = Angle(rendezvous).Degrees();
	positionTime += AngleDiff(currentAngle, positionAngle) / turn;
	positionTime += (rendezvous.Unit() * maxV - s.velocity).Length() / accel;
	// If you are very close, stop trying to adjust:
	positionTime *= positionWeight * positionWeight;

	// Time it will take (roughly) to adjust your velocity to match the target:
	double velocityTime = velocityDelta.Length() / accel;
	double velocityAngle = Angle(velocityDelta).Degrees();
	velocityTime += AngleDiff(currentAngle, velocityAngle) / turn;
	// If you are very close, stop trying to adjust:
	velocityTime *= velocityWeight * velocityWeight;

	// Focus on matching position or velocity depending on which will take longer.
	double totalTime = positionTime + velocityTime + TIME_DEADBAND;
	positionWeight = positionTime / totalTime;
	velocityWeight = velocityTime / totalTime;
	double facingWeight = TIME_DEADBAND / totalTime;

	// Determine the angle we want to face, interpolating smoothly between three options.
	Point facingGoal = rendezvous.Unit() * positionWeight
		+ velocityDelta.Unit() * velocityWeight
		+ s.targetFacing.Unit() * facingWeight;
	double targetAngle = Angle(facingGoal).Degrees() - currentAngle;
	if(abs(targetAngle) > 180.)
		targetAngle += (targetAngle < 0. ? 360. : -360.);
	// Avoid "turn jitter" when position & velocity are well-matched.
	bool changedDirection = (std::signbit(s.lastTurn) != std::signbit(targetAngle));
	double targetTurn = abs(targetAngle / turn);
	double lastTurn = abs(s.lastTurn);
	if(lastTurn && (changedDirection || (lastTurn < 1. && targetTurn > lastTurn)))
	{
		// Keep the desired turn direction, but damp the per-frame turn rate increase.
		double dampedTurn = (changedDirection ? 0. : lastTurn) + std::min(.025, targetTurn);
		command.turn = copysign(dampedTurn, targetAngle);
	}
	else if(targetTurn < 1.)
		command.turn = copysign(targetTurn, targetAngle);
	else
		command.turn = targetAngle;

	// Determine whether to apply thrust.
	Point drag = s.velocity * s.dragForce;
	if(s.reverseThrust)
	{
		// Don't take drag into account when reverse thrusting, because this
		// estimate of how it will be applied can be quite inaccurate.
		Point a = (unit * (-s.reverseThrust / mass)).Unit();
		double direction = positionWeight * positionDelta.Dot(a) / POSITION_DEADBAND
			+ velocityWeight * velocityDelta.Dot(a) / VELOCITY_DEADBAND;
		if(direction > THRUST_DEADBAND)
		{
			command.back = true;
			return command;
		}
	}
	Point a = (unit * accel - drag).Unit();
	double direction = positionWeight * positionDelta.Dot(a) / POSITION_DEADBAND
		+ velocityWeight * velocityDelta.Dot(a) / VELOCITY_DEADBAND;
	if(direction > THRUST_DEADBAND)
		command.forward = true;
	return command;
}

Steering Scripted(const ShipManeuver &maneuver, const Situation &s)
{
	ShipManeuver::Input input;
	input.position = s.position;
	input.velocity = s.velocity;
	input.facing = s.facing.Unit();
	input.facingDegrees = s.facing.Degrees();
	input.targetPosition = s.targetPosition;
	input.targetVelocity = s.targetVelocity;
	input.targetFacing = s.targetFacing.Unit();
	input.maxVelocity = s.maxVelocity;
	input.acceleration = s.acceleration;
	input.turnRate = s.turnRate;
	input.inertialMass = s.inertialMass;
	input.dragForce = s.dragForce;
	input.reverseThrust = s.reverseThrust;
	input.lastTurn = s.lastTurn;
	ShipManeuver::Output output;
	if(!maneuver.Run(input, output))
		return {std::nan(""), false, false};
	return {output.turn, output.forward, output.back};
}

bool Same(double a, double b)
{
	if(std::isnan(a) && std::isnan(b))
		return true;
	uint64_t x = 0;
	uint64_t y = 0;
	std::memcpy(&x, &a, sizeof(x));
	std::memcpy(&y, &b, sizeof(y));
	return x == y;
}

std::string Describe(const Situation &s)
{
	std::ostringstream out;
	out.precision(17);
	out << "position " << s.position.X() << "," << s.position.Y()
		<< " velocity " << s.velocity.X() << "," << s.velocity.Y()
		<< " facing " << s.facing.Degrees()
		<< " target " << s.targetPosition.X() << "," << s.targetPosition.Y()
		<< " target velocity " << s.targetVelocity.X() << "," << s.targetVelocity.Y()
		<< " target facing " << s.targetFacing.Degrees()
		<< " max v " << s.maxVelocity << " accel " << s.acceleration << " turn " << s.turnRate
		<< " mass " << s.inertialMass << " drag " << s.dragForce << " reverse " << s.reverseThrust
		<< " last turn " << s.lastTurn;
	return out.str();
}

// Load the game's maneuver scripts. Tests run from the "tests" directory.
const ShipManeuver *LoadKeepStation()
{
	static ShipManeuver maneuver;
	static bool loaded = false;
	if(loaded)
		return maneuver.IsValid() ? &maneuver : nullptr;
	loaded = true;
	for(const char *path : {"../data/maneuvers.txt", "data/maneuvers.txt"})
	{
		if(!std::filesystem::exists(path))
			continue;
		DataFile file(path);
		for(const DataNode &node : file)
			if(node.Token(0) == "maneuver" && node.Size() >= 2 && node.Token(1) == "keep station")
				maneuver.Load(node);
		break;
	}
	return maneuver.IsValid() ? &maneuver : nullptr;
}

// Generate situations like an escort near its parent, plus awkward ones:
// sitting exactly on the target, matching its velocity exactly, no reverse
// thrusters, and last turns of exactly 0, -0, and +-1.
class SituationGenerator {
public:
	explicit SituationGenerator(uint64_t seed) : gen(seed) {}

	Situation Next()
	{
		Situation s;
		s.position = RandomPoint(3000.);
		s.velocity = Pick(8) ? Point() : RandomPoint(12.);
		s.facing = Angle(Uniform(0., 360.));
		int placement = Int(5);
		if(placement == 0)
			s.targetPosition = s.position;
		else if(placement == 1)
			s.targetPosition = s.position + RandomPoint(300.);
		else
			s.targetPosition = s.position + RandomPoint(5000.);
		s.targetVelocity = Pick(5) ? s.velocity : (Pick(6) ? Point() : RandomPoint(10.));
		s.targetFacing = Angle(Uniform(0., 360.));
		s.maxVelocity = Uniform(2., 15.);
		s.acceleration = Uniform(.05, 1.);
		s.turnRate = Uniform(1., 12.);
		s.inertialMass = Uniform(50., 5000.);
		s.dragForce = Uniform(.001, .1);
		s.reverseThrust = Pick(2) ? 0. : Uniform(1., 300.);
		int last = Int(6);
		s.lastTurn = last == 0 ? 0. : last == 1 ? -0. : last == 2 ? (Pick(2) ? 1. : -1.) : Uniform(-1., 1.);
		return s;
	}


private:
	double Uniform(double low, double high) { return std::uniform_real_distribution<double>(low, high)(gen); }
	int Int(int count) { return std::uniform_int_distribution<int>(0, count - 1)(gen); }
	// True with a probability of one in the given count.
	bool Pick(int count) { return !Int(count); }
	Point RandomPoint(double size) { return Point(Uniform(-size, size), Uniform(-size, size)); }


private:
	std::mt19937_64 gen;
};
// #endregion mock data



// #region unit tests
TEST_CASE( "The keep station maneuver matches the built-in AI code", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadKeepStation();
	REQUIRE( maneuver );

	SituationGenerator generator(42);
	int mismatches = 0;
	int forward = 0;
	int back = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		const Situation s = generator.Next();
		const Steering expected = BuiltInKeepStation(s);
		const Steering actual = Scripted(*maneuver, s);
		forward += expected.forward;
		back += expected.back;
		if(!Same(expected.turn, actual.turn) || expected.forward != actual.forward || expected.back != actual.back)
		{
			++mismatches;
			UNSCOPED_INFO( Describe(s) );
			UNSCOPED_INFO( "expected turn " << expected.turn << " forward " << expected.forward << " back "
				<< expected.back << "; got turn " << actual.turn << " forward " << actual.forward
				<< " back " << actual.back );
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( forward > 0 );
	CHECK( back > 0 );
}
// #endregion unit tests



} // test namespace
