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
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

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
	bool shouldUseAfterburner = false;
	// For "move to":
	double topSpeed = 0.;
	double radius = 0.;
	double slow = 0.;
	double cruiseSpeed = 0.;
	double commandTurn = 0.;
	// For "attack":
	Point aim;
};

struct Steering {
	double turn = 0.;
	bool forward = false;
	bool back = false;
	bool afterburner = false;
	bool arrived = false;
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

// AI::TurnToward and AI::CircleAround from before the "circle around"
// maneuver script replaced the latter, changed only to read from a Situation.
double BuiltInTurnToward(const Situation &s, const Point &vector, const double precision = 0.9999)
{
	Point facing = s.facing.Unit();
	double cross = vector.Cross(facing);

	double dot = vector.Dot(facing);
	if(dot > 0.)
	{
		// Is the facing direction aligned with the target direction with sufficient precision?
		// The maximum angle between the two directions is given by: arccos(sqrt(precision)).
		bool close = false;
		if(precision < 1. && precision > 0. && dot * dot >= precision * vector.LengthSquared())
			close = true;
		double angle = asin(std::min(1., std::max(-1., cross / vector.Length()))) * TO_DEG;
		// Is the angle between the facing and target direction smaller than
		// the angle the ship can turn through in one step?
		if(fabs(angle) < s.turnRate)
		{
			// If the ship is within one step of the target direction,
			// and the facing is already sufficiently aligned with the target direction,
			// don't turn any further.
			if(close)
				return 0.;
			return -angle / s.turnRate;
		}
	}

	bool left = cross < 0.;
	return left - !left;
}

Steering BuiltInCircleAround(const Situation &s)
{
	Steering command;
	Point direction = s.targetPosition - s.position;
	command.turn = BuiltInTurnToward(s, direction);

	double length = direction.Length();
	if(length > 200. && s.facing.Unit().Dot(direction) >= 0.)
	{
		command.forward = true;

		// If the ship is far away enough the ship should use the afterburner.
		if(length > 750. && s.shouldUseAfterburner)
			command.afterburner = true;
	}
	return command;
}

// The ship calculations "move to" relies on, which stay in C++. The script and
// the copy of AI::MoveTo below both use these, so the test checks the script's
// own logic. (This copy of AI::StoppingPoint leaves out hyperspace.)
class TestContext : public ShipManeuver::Context {
public:
	explicit TestContext(const Situation &s) : s(s) {}

	Point StoppingPoint(const Point &targetVelocity, bool &shouldReverse) const override
	{
		Point position = s.position;
		Point velocity = s.velocity - targetVelocity;
		shouldReverse = false;
		double v = velocity.Length();
		if(!v)
			return position;
		double degreesToTurn = TO_DEG * acos(std::min(1., std::max(-1., -velocity.Unit().Dot(s.facing.Unit()))));
		double stopDistance = v * (degreesToTurn / s.turnRate);
		stopDistance += .5 * v * v / s.acceleration;
		if(s.reverseThrust)
		{
			double reverseAcceleration = s.reverseThrust / s.inertialMass;
			double reverseDistance = v * (180. - degreesToTurn) / s.turnRate;
			reverseDistance += .5 * v * v / reverseAcceleration;
			if(reverseDistance < stopDistance)
			{
				shouldReverse = true;
				stopDistance = reverseDistance;
			}
		}
		return position + stopDistance * velocity.Unit();
	}

	double TurnToward(const Point &vector) const override
	{
		return BuiltInTurnToward(s, vector);
	}


private:
	const Situation &s;
};

// AI::MoveTo from before the "move to" maneuver script replaced it, changed
// only to read from a Situation and use the context's ship calculations.
Steering BuiltInMoveTo(const Situation &s, const ShipManeuver::Context &context)
{
	Steering command;
	command.turn = s.commandTurn;
	const Point &targetPosition = s.targetPosition;
	const Point &targetVelocity = s.targetVelocity;
	const double radius = s.radius;
	const double slow = s.slow;
	const double cruiseSpeed = s.cruiseSpeed;

	const Point &position = s.position;
	const Point &velocity = s.velocity;
	const Angle &angle = s.facing;
	Point dp = targetPosition - position;
	Point dv = targetVelocity - velocity;

	double speed = dv.Length();

	bool isClose = (dp.Length() < radius);
	if(isClose && speed < slow)
	{
		command.arrived = true;
		return command;
	}

	bool shouldReverse = false;
	dp = targetPosition - context.StoppingPoint(targetVelocity, shouldReverse);

	// Calculate target vector required to get where we want to be.
	Point tv = dp;
	bool hasCruiseSpeed = (cruiseSpeed > 0.);
	if(hasCruiseSpeed)
	{
		// The ship prefers a velocity at cruise-speed towards the target, so we need
		// to compare this preferred velocity to the current velocity and apply the
		// delta to get to the preferred velocity.
		tv = (dp.Unit() * cruiseSpeed) - velocity;
		// If we are moving close to our preferred velocity, then face towards the target.
		if(tv.LengthSquared() < .01)
			tv = dp;
	}

	bool isFacing = (tv.Unit().Dot(angle.Unit()) > .95);
	if(!isClose || (!isFacing && !shouldReverse))
		command.turn = context.TurnToward(tv);

	// Drag is not applied when not thrusting, so stop thrusting when close to max speed
	// to save energy. Work with a slightly lower maximum velocity to avoid border cases.
	double maxVelocity = s.topSpeed * .99;
	if(isFacing && (velocity.LengthSquared() <= maxVelocity * maxVelocity
			|| dp.Unit().Dot(velocity.Unit()) < .95))
	{
		// We set full forward power when we don't have a cruise-speed, when we are below
		// cruise-speed or when we need to do course corrections.
		bool movingTowardsTarget = (velocity.Unit().Dot(dp.Unit()) > .95);
		if(!hasCruiseSpeed || !movingTowardsTarget || velocity.Length() < cruiseSpeed)
			command.forward = true;
	}
	else if(shouldReverse)
	{
		command.turn = context.TurnToward(velocity);
		command.back = true;
	}

	return command;
}

// AI::PickUp from before the "pick up" maneuver script replaced it, changed
// only to read from a Situation and use the context's ship calculations.
Steering BuiltInPickUp(const Situation &s, const ShipManeuver::Context &context)
{
	Steering command;
	command.turn = s.commandTurn;
	// Figure out the target's velocity relative to the ship.
	Point p = s.targetPosition - s.position;
	Point v = s.targetVelocity - s.velocity;
	double vMax = s.maxVelocity;

	// Estimate where the target will be by the time we reach it.
	double time = AI::RendezvousTime(p, v, vMax);
	if(std::isnan(time))
		time = p.Length() / vMax;
	double degreesToTurn = TO_DEG * acos(std::min(1., std::max(-1., p.Unit().Dot(s.facing.Unit()))));
	time += degreesToTurn / s.turnRate;
	p += v * time;

	// Move toward the target.
	command.turn = context.TurnToward(p);
	double dp = p.Unit().Dot(s.facing.Unit());
	if(dp > .7)
		command.forward = true;

	// Use the afterburner if it will not cause you to miss your target.
	double squareDistance = p.LengthSquared();
	if(command.forward && s.shouldUseAfterburner)
		if(dp > std::max(.9, std::min(.9999, 1. - squareDistance / 10000000.)))
			command.afterburner = true;
	return command;
}

// AI::MoveToAttack from before the "attack" maneuver script replaced it,
// changed only to read from a Situation and use the context's ship
// calculations. The aim comes from AI::TargetAim, which stays in C++.
Steering BuiltInMoveToAttack(const Situation &s, const ShipManeuver::Context &context)
{
	Steering command;
	Point direction = s.targetPosition - s.position;

	// First of all, aim in the direction that will hit this target.
	command.turn = context.TurnToward(s.aim);

	// Calculate this ship's "turning radius"; that is, the smallest circle it
	// can make while at its current speed.
	double stepsInFullTurn = 360. / s.turnRate;
	double circumference = stepsInFullTurn * s.velocity.Length();
	double diameter = std::max(200., circumference / PI);

	const auto facing = s.facing.Unit().Dot(direction.Unit());
	// If the ship has reverse thrusters and the target is behind it, we can
	// use them to reach the target more quickly.
	if(facing < -.75 && s.reverseThrust)
		command.back = true;
	// Only apply thrust if either:
	// This ship is within 90 degrees of facing towards its target and far enough away not to overshoot
	// if it accelerates while needing to turn further, or:
	// This ship is moving away from its target but facing mostly towards it.
	else if((facing >= 0. && direction.Length() > diameter)
			|| (s.velocity.Dot(direction) < 0. && facing >= .9))
	{
		command.forward = true;
		// Use afterburner, if applicable.
		if(direction.Length() > 600. && s.shouldUseAfterburner)
			command.afterburner = true;
	}
	return command;
}

Steering Scripted(const ShipManeuver &maneuver, const Situation &s, const ShipManeuver::Context *context = nullptr)
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
	input.shouldUseAfterburner = s.shouldUseAfterburner;
	input.topSpeed = s.topSpeed;
	input.radius = s.radius;
	input.slow = s.slow;
	input.cruiseSpeed = s.cruiseSpeed;
	input.commandTurn = s.commandTurn;
	input.aim = s.aim;
	ShipManeuver::Output output;
	if(!maneuver.Run(input, output, context))
		return {std::nan(""), false, false, false, false};
	return {output.turn, output.forward, output.back, output.afterburner, output.arrived};
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

// Load one of the game's maneuver scripts. Tests run from the "tests" directory.
const ShipManeuver *LoadManeuver(const std::string &name)
{
	static std::map<std::string, ShipManeuver> maneuvers;
	if(maneuvers.empty())
		for(const char *path : {"../data/maneuvers.txt", "data/maneuvers.txt"})
		{
			if(!std::filesystem::exists(path))
				continue;
			DataFile file(path);
			for(const DataNode &node : file)
				if(node.Token(0) == "maneuver" && node.Size() >= 2)
					maneuvers[node.Token(1)].Load(node);
			break;
		}
	auto it = maneuvers.find(name);
	return it != maneuvers.end() && it->second.IsValid() ? &it->second : nullptr;
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
		s.shouldUseAfterburner = Pick(2);
		s.topSpeed = s.maxVelocity * (Pick(3) ? 1.5 : 1.);
		s.radius = Pick(4) ? 0. : Uniform(10., 800.);
		s.slow = Uniform(0., 5.);
		s.cruiseSpeed = Pick(2) ? 0. : Uniform(.5, s.maxVelocity);
		s.commandTurn = Pick(2) ? 0. : Uniform(-1., 1.);
		// Usually near the direction to the target, as when leading it.
		s.aim = (s.targetPosition - s.position) + RandomPoint(Pick(2) ? 50. : 1000.);
		// Sometimes put the target exactly ahead or behind.
		if(Pick(10))
			s.targetPosition = s.position + s.facing.Unit() * (Pick(2) ? Uniform(1., 2000.) : -Uniform(1., 2000.));
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
	const ShipManeuver *maneuver = LoadManeuver("keep station");
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
				<< " back " << actual.back);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( forward > 0 );
	CHECK( back > 0 );
}

TEST_CASE( "The circle around maneuver matches the built-in AI code", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadManeuver("circle around");
	REQUIRE( maneuver );

	SituationGenerator generator(7);
	int mismatches = 0;
	int stopped = 0;
	int afterburner = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		const Situation s = generator.Next();
		const Steering expected = BuiltInCircleAround(s);
		const Steering actual = Scripted(*maneuver, s);
		stopped += expected.turn == 0.;
		afterburner += expected.afterburner;
		if(!Same(expected.turn, actual.turn) || expected.forward != actual.forward || expected.back != actual.back
				|| expected.afterburner != actual.afterburner)
		{
			++mismatches;
			UNSCOPED_INFO( Describe(s) );
			UNSCOPED_INFO( "expected turn " << expected.turn << " forward " << expected.forward << " afterburner "
				<< expected.afterburner << "; got turn " << actual.turn << " forward " << actual.forward
				<< " afterburner " << actual.afterburner);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( stopped > 0 );
	CHECK( afterburner > 0 );
}

TEST_CASE( "The move to maneuver matches the built-in AI code", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadManeuver("move to");
	REQUIRE( maneuver );

	SituationGenerator generator(11);
	int mismatches = 0;
	int arrived = 0;
	int forward = 0;
	int back = 0;
	int kept = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		Situation s = generator.Next();
		// Sometimes start already there, or nearly there.
		if(generator.Next().shouldUseAfterburner)
			s.targetPosition = s.position + Point(s.radius * .5, 0.);
		TestContext context(s);
		const Steering expected = BuiltInMoveTo(s, context);
		const Steering actual = Scripted(*maneuver, s, &context);
		arrived += expected.arrived;
		forward += expected.forward;
		back += expected.back;
		kept += (!expected.arrived && expected.turn == s.commandTurn);
		if(!Same(expected.turn, actual.turn) || expected.forward != actual.forward || expected.back != actual.back
				|| expected.arrived != actual.arrived || actual.afterburner)
		{
			++mismatches;
			UNSCOPED_INFO( Describe(s) << " radius " << s.radius << " slow " << s.slow << " cruise "
				<< s.cruiseSpeed << " top speed " << s.topSpeed << " command turn " << s.commandTurn);
			UNSCOPED_INFO( "expected turn " << expected.turn << " forward " << expected.forward << " back "
				<< expected.back << " arrived " << expected.arrived << "; got turn " << actual.turn << " forward "
				<< actual.forward << " back " << actual.back << " arrived " << actual.arrived);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( arrived > 0 );
	CHECK( forward > 0 );
	CHECK( back > 0 );
	CHECK( kept > 0 );
}

TEST_CASE( "The pick up maneuver matches the built-in AI code", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadManeuver("pick up");
	REQUIRE( maneuver );

	SituationGenerator generator(13);
	int mismatches = 0;
	int forward = 0;
	int afterburner = 0;
	int neverMeets = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		Situation s = generator.Next();
		// Sometimes the flotsam drifts away faster than the ship can fly.
		if(generator.Next().shouldUseAfterburner)
			s.targetVelocity = s.velocity + (s.targetPosition - s.position).Unit() * (s.maxVelocity * 1.5);
		TestContext context(s);
		const Steering expected = BuiltInPickUp(s, context);
		const Steering actual = Scripted(*maneuver, s, &context);
		Point p = s.targetPosition - s.position;
		neverMeets += std::isnan(AI::RendezvousTime(p, s.targetVelocity - s.velocity, s.maxVelocity));
		forward += expected.forward;
		afterburner += expected.afterburner;
		if(!Same(expected.turn, actual.turn) || expected.forward != actual.forward
				|| expected.afterburner != actual.afterburner || actual.back || actual.arrived)
		{
			++mismatches;
			UNSCOPED_INFO( Describe(s) << " command turn " << s.commandTurn );
			UNSCOPED_INFO( "expected turn " << expected.turn << " forward " << expected.forward << " afterburner "
				<< expected.afterburner << "; got turn " << actual.turn << " forward " << actual.forward
				<< " afterburner " << actual.afterburner);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( forward > 0 );
	CHECK( afterburner > 0 );
	CHECK( neverMeets > 0 );
}

TEST_CASE( "The attack maneuver matches the built-in AI code", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadManeuver("attack");
	REQUIRE( maneuver );

	SituationGenerator generator(17);
	int mismatches = 0;
	int forward = 0;
	int back = 0;
	int afterburner = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		Situation s = generator.Next();
		// Sometimes stand still, or turn slowly, so the turning circle is small.
		if(generator.Next().shouldUseAfterburner)
			s.velocity = Point();
		TestContext context(s);
		const Steering expected = BuiltInMoveToAttack(s, context);
		const Steering actual = Scripted(*maneuver, s, &context);
		forward += expected.forward;
		back += expected.back;
		afterburner += expected.afterburner;
		if(!Same(expected.turn, actual.turn) || expected.forward != actual.forward || expected.back != actual.back
				|| expected.afterburner != actual.afterburner || actual.arrived)
		{
			++mismatches;
			UNSCOPED_INFO( Describe(s) << " aim " << s.aim.X() << "," << s.aim.Y() );
			UNSCOPED_INFO( "expected turn " << expected.turn << " forward " << expected.forward << " back "
				<< expected.back << " afterburner " << expected.afterburner << "; got turn " << actual.turn
				<< " forward " << actual.forward << " back " << actual.back << " afterburner " << actual.afterburner);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations reach every branch.
	CHECK( forward > 0 );
	CHECK( back > 0 );
	CHECK( afterburner > 0 );
}

// Not run by default: "endless-sky-tests [.maneuver-speed]" times the scripts.
TEST_CASE( "Maneuver script speed", "[.maneuver-speed]" ) {
	const ShipManeuver *keepStation = LoadManeuver("keep station");
	const ShipManeuver *moveTo = LoadManeuver("move to");
	REQUIRE( keepStation );
	REQUIRE( moveTo );
	SituationGenerator generator(3);
	std::vector<Situation> situations;
	for(int i = 0; i < 1000; ++i)
		situations.push_back(generator.Next());
	double total = 0.;
	for(int round = 0; round < 20; ++round)
		for(const Situation &s : situations)
		{
			TestContext context(s);
			total += Scripted(*keepStation, s).turn + Scripted(*moveTo, s, &context).turn;
		}
	CHECK( !std::isnan(total) );
}

TEST_CASE( "Ship functions need a ship", "[ShipManeuver]" ) {
	const ShipManeuver *maneuver = LoadManeuver("move to");
	REQUIRE( maneuver );
	Situation s;
	s.targetPosition = Point(1000., 0.);
	s.acceleration = 1.;
	s.turnRate = 1.;
	THEN( "running without a context fails instead of guessing" ) {
		CHECK( std::isnan(Scripted(*maneuver, s).turn) );
	}
}
// #endregion unit tests



} // test namespace
