/* test_turretScript.cpp
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
#include "../../../source/TurretScript.h"

// Include helpers for reading data files, and the random numbers the script uses.
#include "../../../source/DataFile.h"
#include "../../../source/DataNode.h"
#include "../../../source/Random.h"

#include <filesystem>
#include <random>
#include <string>

namespace { // test namespace

// #region mock data
// The idle sweep from AI::AimTurrets before the "idle sweep" turret script
// replaced it, changed only to take its inputs as arguments and return the
// aim instead of setting it. (A turret that stays at rest keeps an aim of 0.)
double BuiltInIdleSweep(double previous, double idleOffset, double arcMiddleDegrees)
{
	if(!previous && Random::Int(60))
		return previous;

	double bias = idleOffset / std::min(arcMiddleDegrees, 180.);
	double acceleration = Random::Real() - Random::Real() + bias;
	return previous + .1 * acceleration;
}

// Load one of the game's turret scripts. Tests run from the "tests" directory.
const TurretScript *LoadTurretScript(const std::string &name)
{
	static TurretScript script;
	static bool loaded = false;
	if(!loaded)
		for(const char *path : {"../data/turrets.txt", "data/turrets.txt"})
		{
			if(!std::filesystem::exists(path))
				continue;
			DataFile file(path);
			for(const DataNode &node : file)
				if(node.Token(0) == "turret" && node.Size() >= 2 && node.Token(1) == name)
					script.Load(node);
			loaded = true;
			break;
		}
	return script.IsValid() ? &script : nullptr;
}
// #endregion mock data



// #region unit tests
TEST_CASE( "The idle sweep turret script matches the built-in AI code", "[TurretScript]" ) {
	const TurretScript *script = LoadTurretScript("idle sweep");
	REQUIRE( script );

	std::mt19937_64 gen(29);
	auto uniform = [&gen](double low, double high) { return std::uniform_real_distribution<double>(low, high)(gen); };
	int mismatches = 0;
	int moved = 0;
	int started = 0;
	for(int i = 0; i < 50000 && mismatches < 5; ++i)
	{
		TurretScript::Input input;
		// Aims are stored in steps of 1/127, and are often 0 (at rest).
		input.previousAim = (i % 2) ? 0. : std::round(uniform(-127., 127.)) / 127.;
		input.idleOffset = uniform(-180., 180.);
		input.arcMiddle = (i % 3) ? 180. : uniform(5., 180.);

		// Give both the same random numbers, and check that they also use the
		// same number of them.
		Random::Seed(i);
		const double expected = BuiltInIdleSweep(input.previousAim, input.idleOffset, input.arcMiddle);
		const uint32_t expectedNext = Random::Int();
		Random::Seed(i);
		double actual = std::nan("");
		REQUIRE( script->Run(input, actual) );
		const uint32_t actualNext = Random::Int();

		moved += (expected != input.previousAim);
		started += (!input.previousAim && expected);
		if(expected != actual || expectedNext != actualNext)
		{
			++mismatches;
			UNSCOPED_INFO( "previous " << input.previousAim << " offset " << input.idleOffset << " arc "
				<< input.arcMiddle << ": expected " << expected << ", got " << actual);
		}
	}
	CHECK( mismatches == 0 );
	CHECK( moved > 0 );
	CHECK( started > 0 );
}
// #endregion unit tests



} // test namespace
