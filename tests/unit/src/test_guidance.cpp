/* test_guidance.cpp
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
#include "../../../source/MissileGuidance.h"

// Include helpers for creating well-formed DataNodes and reading data files.
#include "datanode-factory.h"
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

namespace { // test namespace

// #region mock data
// Everything the built-in homing code depended on.
struct Situation {
	Point position;
	Point velocity;
	Point facing;
	bool hasTarget = false;
	Point targetPosition;
	Point targetVelocity;
	bool hasLock = false;
	bool confused = false;
	int confusionDirection = 1;
	double turn = 0.;
	double accel = 0.;
	double drag = 0.;
	bool blindspot = false;
	bool leading = false;
	bool throttleControl = false;
};

struct Steering {
	double turn = 0.;
	double accel = 0.;
	bool dropTarget = false;
};

// The homing code from Projectile::Move before guidance scripts replaced it,
// changed only to read from a Situation. This is the reference that the
// default scripts must match exactly. If the game's homing code changes, copy
// the new version here, and update data/guidance.txt until this test passes.
Steering BuiltInHoming(const Situation &s)
{
	double turn = s.turn;
	double accel = s.accel;
	bool dropTarget = false;

	// Vector d is the direction we want to turn towards.
	Point d;
	bool isFacingAway = false;
	if(s.hasTarget)
	{
		d = s.targetPosition - s.position;
		isFacingAway = d.Dot(s.facing) < 0.;
	}

	// The very dumbest of homing missiles lose their target if pointed
	// away from it.
	if(isFacingAway && s.blindspot)
		dropTarget = true;
	else if(s.hasTarget && s.hasLock)
	{
		Point unit = d.Unit();
		double drag = s.drag;
		double trueVelocity = drag ? accel / drag : s.velocity.Length();
		double stepsToReach = d.Length() / trueVelocity;
		// At the highest homing level, compensate for target motion.
		if(s.leading)
		{
			if(unit.Dot(s.targetVelocity) < 0.)
			{
				// If the target is moving toward this projectile, the intercept
				// course is where the target and the projectile have the same
				// velocity normal to the distance between them.
				Point normal(unit.Y(), -unit.X());
				double vN = normal.Dot(s.targetVelocity);
				double vT = sqrt(std::max(0., trueVelocity * trueVelocity - vN * vN));
				d = vT * unit + vN * normal;
			}
			else
			{
				// Adjust the target's position based on where it will be when we
				// reach it (assuming we're pointed right towards it).
				d += stepsToReach * s.targetVelocity;
				stepsToReach = d.Length() / trueVelocity;
			}
			unit = d.Unit();
		}

		double cross = s.facing.Cross(unit);

		double desiredTurn = TO_DEG * asin(cross);
		if(fabs(desiredTurn) > turn)
			turn = copysign(turn, desiredTurn);
		else
			turn = desiredTurn;

		// Levels 3 and 4 stop accelerating when facing away.
		if(s.throttleControl)
		{
			double stepsToFace = desiredTurn / turn;

			// If you are facing away from the target, stop accelerating.
			if(stepsToFace * 1.5 > stepsToReach)
				accel = 0.;
		}
	}
	// Turn in a random direction if this weapon is confused.
	else if(s.confused)
		turn *= s.confusionDirection;
	// If a weapon is homing but has no target, do not turn it.
	else
		turn = 0.;

	return {turn, accel, dropTarget};
}

// The same decision, made the way Projectile::Steer makes it now: the seeker
// handles the blind spot, then the weapon's default script runs.
Steering Scripted(const MissileGuidance &guidance, const Situation &s)
{
	if(s.hasTarget && s.blindspot && (s.targetPosition - s.position).Dot(s.facing) < 0.)
		return {s.turn, s.accel, true};

	MissileGuidance::Input input;
	input.position = s.position;
	input.velocity = s.velocity;
	input.facing = s.facing;
	input.hasLock = s.hasTarget && s.hasLock;
	input.targetPosition = s.targetPosition;
	input.targetVelocity = s.targetVelocity;
	input.confused = s.confused;
	input.confusionDirection = s.confusionDirection;
	input.maxTurn = s.turn;
	input.acceleration = s.accel;
	input.drag = s.drag;
	MissileGuidance::Output output;
	if(!guidance.Run(input, output))
		return {0., s.accel, false};
	return {output.turn, output.thrust, output.dropTarget};
}

// Doubles are the same if they have the same bits, or are both not numbers.
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
		<< " facing " << s.facing.X() << "," << s.facing.Y()
		<< " target " << s.hasTarget << " " << s.targetPosition.X() << "," << s.targetPosition.Y()
		<< " target velocity " << s.targetVelocity.X() << "," << s.targetVelocity.Y()
		<< " lock " << s.hasLock << " confused " << s.confused << " direction " << s.confusionDirection
		<< " turn " << s.turn << " accel " << s.accel << " drag " << s.drag
		<< " blindspot " << s.blindspot << " leading " << s.leading << " throttle " << s.throttleControl;
	return out.str();
}

// Load the game's guidance scripts. Tests run from the "tests" directory.
std::map<std::string, MissileGuidance> LoadGameGuidance()
{
	std::map<std::string, MissileGuidance> scripts;
	for(const char *path : {"../data/guidance.txt", "data/guidance.txt"})
	{
		if(!std::filesystem::exists(path))
			continue;
		DataFile file(path);
		for(const DataNode &node : file)
			if(node.Token(0) == "guidance")
				for(int i = 1; i < node.Size(); ++i)
					scripts[node.Token(i)].Load(node, node.Token(i));
		break;
	}
	return scripts;
}

// Generate situations, including the awkward ones: targets exactly ahead,
// exactly behind, or on top of the projectile, zero drag and zero speed.
class SituationGenerator {
public:
	explicit SituationGenerator(uint64_t seed) : gen(seed) {}

	Situation Next()
	{
		Situation s;
		s.position = RandomPoint(5000.);
		s.velocity = Pick(8) ? Point() : RandomPoint(20.);
		double angle = Uniform(0., 2. * PI);
		s.facing = Point(sin(angle), -cos(angle));
		s.hasTarget = Pick(6);
		int placement = Int(6);
		if(placement == 0)
			s.targetPosition = s.position;
		else if(placement == 1)
			s.targetPosition = s.position + Uniform(1., 3000.) * s.facing;
		else if(placement == 2)
			s.targetPosition = s.position - Uniform(1., 3000.) * s.facing;
		else
			s.targetPosition = s.position + RandomPoint(3000.);
		s.targetVelocity = Pick(6) ? Point() : RandomPoint(15.);
		s.hasLock = !Pick(4);
		s.confused = Pick(3);
		s.confusionDirection = Pick(2) ? -1 : 1;
		s.turn = Pick(10) ? Uniform(10., 30.) : Uniform(0.01, 8.);
		s.accel = Uniform(0.01, 3.);
		s.drag = Pick(3) ? 0. : Uniform(0.001, 0.2);
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
TEST_CASE( "Default guidance scripts match the built-in homing code", "[MissileGuidance]" ) {
	const auto scripts = LoadGameGuidance();
	REQUIRE( !scripts.empty() );

	// Every combination of homing attributes.
	for(int capabilities = 0; capabilities < 8; ++capabilities)
	{
		const bool blindspot = capabilities & 1;
		const bool leading = capabilities & 2;
		const bool throttleControl = capabilities & 4;
		const std::string name = MissileGuidance::DefaultName(leading, throttleControl);
		CAPTURE( name, blindspot );
		auto it = scripts.find(name);
		REQUIRE( it != scripts.end() );
		const MissileGuidance &guidance = it->second;
		REQUIRE( guidance.IsValid() );
		REQUIRE( guidance.MissingCapability(leading, throttleControl).empty() );

		SituationGenerator generator(capabilities);
		int mismatches = 0;
		for(int i = 0; i < 20000 && mismatches < 5; ++i)
		{
			Situation s = generator.Next();
			s.blindspot = blindspot;
			s.leading = leading;
			s.throttleControl = throttleControl;
			const Steering expected = BuiltInHoming(s);
			const Steering actual = Scripted(guidance, s);
			if(!Same(expected.turn, actual.turn) || !Same(expected.accel, actual.accel)
					|| expected.dropTarget != actual.dropTarget)
			{
				++mismatches;
				UNSCOPED_INFO( Describe(s) );
				UNSCOPED_INFO( "expected turn " << expected.turn << " accel " << expected.accel
					<< " drop " << expected.dropTarget << "; got turn " << actual.turn
					<< " accel " << actual.accel << " drop " << actual.dropTarget );
			}
		}
		CHECK( mismatches == 0 );
	}
}

TEST_CASE( "Guidance scripts need the capabilities they use", "[MissileGuidance]" ) {
	auto load = [](const std::string &text) {
		MissileGuidance guidance;
		guidance.Load(AsDataNode(text), "test");
		return guidance;
	};
	GIVEN( "a script that only turns" ) {
		const MissileGuidance guidance = load("guidance test\n\tset turn (* max-turn 0.5)");
		REQUIRE( guidance.IsValid() );
		THEN( "any homing weapon can use it" ) {
			CHECK( guidance.MissingCapability(false, false).empty() );
		}
	}
	GIVEN( "a script that reads the target's velocity" ) {
		const MissileGuidance guidance = load("guidance test\n\tif (> (length target-velocity) 1)\n\t\tset turn 0");
		REQUIRE( guidance.IsValid() );
		THEN( "only leading weapons can use it" ) {
			CHECK( !guidance.MissingCapability(false, true).empty() );
			CHECK( guidance.MissingCapability(true, false).empty() );
		}
	}
	GIVEN( "a script that sets its thrust" ) {
		const MissileGuidance guidance = load("guidance test\n\tset thrust 0");
		REQUIRE( guidance.IsValid() );
		THEN( "only weapons with throttle control can use it" ) {
			CHECK( !guidance.MissingCapability(true, false).empty() );
			CHECK( guidance.MissingCapability(false, true).empty() );
		}
	}
}

TEST_CASE( "Guidance outputs are limited to what the weapon can do", "[MissileGuidance]" ) {
	MissileGuidance guidance;
	guidance.Load(AsDataNode("guidance test\n\tset turn (* max-turn 3)\n\tset thrust (- 0 5)"), "test");
	REQUIRE( guidance.IsValid() );
	MissileGuidance::Input input;
	input.maxTurn = 4.;
	input.acceleration = 1.;
	MissileGuidance::Output output;
	REQUIRE( guidance.Run(input, output) );
	CHECK( output.turn == 4. );
	CHECK( output.thrust == 0. );
}
// #endregion unit tests



} // test namespace
