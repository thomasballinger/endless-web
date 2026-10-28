/* test_choiceScript.cpp
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
#include "../../../source/ChoiceScript.h"

// Include helpers for reading data files, and the game code the scripts use.
#include "../../../source/AI.h"
#include "../../../source/Angle.h"
#include "../../../source/DataFile.h"
#include "../../../source/DataNode.h"
#include "../../../source/pi.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace { // test namespace

// #region mock data
struct Candidate {
	Point position;
	Point velocity;
};

// The loop from AI::DoHarvesting before the "flotsam" choice scripts
// replaced it, changed only to take its inputs as arguments and return the
// index of the chosen candidate (or -1). The candidates are only those the
// ship can pick up, which is still decided in C++.
int BuiltInChooseFlotsam(const ChoiceScript::Input &ship, const std::vector<Candidate> &flotsam)
{
	int target = -1;
	// Don't chase anything that will take more than 10 seconds to reach.
	double bestTime = 600.;
	for(size_t i = 0; i < flotsam.size(); ++i)
	{
		const Candidate *it = &flotsam[i];
		// Only pick up flotsam that is nearby and that you are facing toward. Player escorts should
		// always attempt to pick up nearby flotsams when they are given a harvest order, and so ignore
		// the facing angle check.
		Point p = it->position - ship.position;
		double range = p.Length();
		// Player ships do not have a restricted field of view so that they target flotsam behind them.
		if(range > 800. || (range > 100. && p.Unit().Dot(ship.facing) < .9 && !ship.yours))
			continue;

		// Estimate how long it would take to intercept this flotsam.
		Point v = it->velocity - ship.velocity;
		double vMax = ship.maxVelocity;
		double time = AI::RendezvousTime(p, v, vMax);
		if(std::isnan(time))
			continue;

		double degreesToTurn = TO_DEG * acos(std::min(1., std::max(-1., p.Unit().Dot(ship.facing))));
		time += degreesToTurn / ship.turnRate;
		if(time < bestTime)
		{
			bestTime = time;
			target = static_cast<int>(i);
		}
	}
	return target;
}

// What the AI does with a "rate" script: rate each candidate, and keep the
// lowest score.
int RateEach(const ChoiceScript &script, ChoiceScript::Input input, const std::vector<Candidate> &flotsam)
{
	int best = -1;
	double bestScore = std::numeric_limits<double>::infinity();
	for(size_t i = 0; i < flotsam.size(); ++i)
	{
		input.candidatePosition = flotsam[i].position;
		input.candidateVelocity = flotsam[i].velocity;
		double score = 0.;
		if(script.Rate(input, score) && score < bestScore)
		{
			bestScore = score;
			best = static_cast<int>(i);
		}
	}
	return best;
}

// What the AI does with a "choose" script: give it the list.
int ChooseFromList(const ChoiceScript &script, ChoiceScript::Input input, const std::vector<Candidate> &flotsam)
{
	// Reuse the list, as the AI does.
	static script::RecordList list(2);
	list.Clear();
	for(const Candidate &candidate : flotsam)
	{
		list.Add();
		list.SetField(0, script::Value(candidate.position));
		list.SetField(1, script::Value(candidate.velocity));
	}
	input.candidates = &list;
	return script.Choose(input);
}

// Load a choice script from a data file. Tests run from the "tests" directory.
ChoiceScript LoadChoice(const std::string &path, const std::string &keyword)
{
	ChoiceScript script;
	for(const std::string prefix : {"../", ""})
	{
		if(!std::filesystem::exists(prefix + path))
			continue;
		DataFile file(prefix + path);
		for(const DataNode &node : file)
			if(node.Token(0) == keyword && node.Size() >= 2 && node.Token(1) == "flotsam")
				script.Load(node, keyword == "rate" ? ChoiceScript::Kind::RATE : ChoiceScript::Kind::CHOOSE);
		break;
	}
	return script;
}

// Ships with flotsam scattered around them: often near or ahead, sometimes
// far or behind, drifting, and sometimes moving faster than the ship can.
class Generator {
public:
	explicit Generator(uint64_t seed) : gen(seed) {}

	ChoiceScript::Input Ship()
	{
		ChoiceScript::Input ship;
		ship.position = Point(Uniform(-3000., 3000.), Uniform(-3000., 3000.));
		ship.velocity = Pick(4) ? Point() : Point(Uniform(-8., 8.), Uniform(-8., 8.));
		ship.facing = Angle(Uniform(0., 360.)).Unit();
		ship.maxVelocity = Uniform(2., 12.);
		ship.turnRate = Uniform(1., 10.);
		ship.yours = Pick(3);
		return ship;
	}

	std::vector<Candidate> Flotsam(const ChoiceScript::Input &ship)
	{
		std::vector<Candidate> flotsam(static_cast<size_t>(Uniform(0., 9.)));
		for(Candidate &candidate : flotsam)
		{
			// Mostly ahead of the ship, within a little more than 800 units.
			double angle = Pick(2) ? Uniform(-30., 30.) : Uniform(0., 360.);
			candidate.position = ship.position
				+ (Angle(ship.facing) + Angle(angle)).Unit() * (Pick(5) ? Uniform(0., 120.) : Uniform(0., 1000.));
			candidate.velocity = Point(Uniform(-1., 1.), Uniform(-1., 1.)) * (Pick(10) ? 20. : 1.);
		}
		return flotsam;
	}


private:
	double Uniform(double low, double high) { return std::uniform_real_distribution<double>(low, high)(gen); }
	bool Pick(int count) { return !std::uniform_int_distribution<int>(0, count - 1)(gen); }


private:
	std::mt19937_64 gen;
};
// #endregion mock data



// #region unit tests
TEST_CASE( "Both flotsam choice scripts match the built-in AI code", "[ChoiceScript]" ) {
	const ChoiceScript rate = LoadChoice("data/choices.txt", "rate");
	const ChoiceScript choose = LoadChoice("tests/ai/plugins/choose-flotsam/data/choices.txt", "choose");
	REQUIRE( rate.IsValid() );
	REQUIRE( choose.IsValid() );

	Generator generator(31);
	int mismatches = 0;
	int chosen = 0;
	int notFirst = 0;
	for(int i = 0; i < 20000 && mismatches < 5; ++i)
	{
		const ChoiceScript::Input ship = generator.Ship();
		const std::vector<Candidate> flotsam = generator.Flotsam(ship);
		const int expected = BuiltInChooseFlotsam(ship, flotsam);
		const int rated = RateEach(rate, ship, flotsam);
		const int listed = ChooseFromList(choose, ship, flotsam);
		chosen += (expected >= 0);
		notFirst += (expected > 0);
		if(expected != rated || expected != listed)
		{
			++mismatches;
			UNSCOPED_INFO( flotsam.size() << " candidates: expected " << expected << ", rated " << rated
				<< ", chosen from the list " << listed);
		}
	}
	CHECK( mismatches == 0 );
	// Make sure the generated situations make real choices.
	CHECK( chosen > 1000 );
	CHECK( notFirst > 1000 );
}

// Not run by default: "endless-sky-tests [.choice-speed]" times both ways.
TEST_CASE( "Choice script speed", "[.choice-speed]" ) {
	const ChoiceScript rate = LoadChoice("data/choices.txt", "rate");
	const ChoiceScript choose = LoadChoice("tests/ai/plugins/choose-flotsam/data/choices.txt", "choose");
	REQUIRE( rate.IsValid() );
	REQUIRE( choose.IsValid() );
	Generator generator(37);
	std::vector<std::pair<ChoiceScript::Input, std::vector<Candidate>>> situations;
	for(int i = 0; i < 2000; ++i)
	{
		ChoiceScript::Input ship = generator.Ship();
		situations.emplace_back(ship, generator.Flotsam(ship));
	}
	using Clock = std::chrono::steady_clock;
	auto time = [&situations](auto choose) {
		const auto start = Clock::now();
		int total = 0;
		for(int round = 0; round < 20; ++round)
			for(const auto &[ship, flotsam] : situations)
				total += choose(ship, flotsam);
		return std::make_pair(std::chrono::duration<double, std::micro>(Clock::now() - start).count()
			/ (20. * situations.size()), total);
	};
	const auto [builtIn, builtInTotal] = time(BuiltInChooseFlotsam);
	const auto [rated, ratedTotal] = time([&rate](const auto &ship, const auto &flotsam) {
		return RateEach(rate, ship, flotsam); });
	const auto [listed, listedTotal] = time([&choose](const auto &ship, const auto &flotsam) {
		return ChooseFromList(choose, ship, flotsam); });
	WARN( "microseconds per choice: C++ " << builtIn << ", rate " << rated << ", choose " << listed );
	CHECK( builtInTotal == ratedTotal );
	CHECK( builtInTotal == listedTotal );
}
// #endregion unit tests



} // test namespace
