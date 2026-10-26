/* test_random.cpp
Copyright (c) 2020 by Benjamin Hauch

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
#include "../../../source/Random.h"

// ... and any system includes needed for the test file.

namespace { // test namespace

// #region mock data
// #endregion mock data



// #region unit tests
TEST_CASE( "Random::Int", "[random][int]") {
	REQUIRE( Random::Int(1) == 0 );
}

TEST_CASE( "Random streams", "[random][stream]" ) {
	// Some numbers from the main sequence, with no streams involved.
	Random::Seed(1234);
	const uint32_t first = Random::Int();
	const uint32_t second = Random::Int();

	GIVEN( "streams are disabled" ) {
		Random::SetUseStreams(false);
		Random::Seed(1234);
		Random::Stream stream;
		THEN( "scopes do nothing" ) {
			{
				Random::Scope scope(stream);
				CHECK( Random::Int() == first );
			}
			CHECK( Random::Int() == second );
			CHECK( !stream.IsSeeded() );
		}
	}
	GIVEN( "streams are enabled" ) {
		Random::SetUseStreams(true);
		Random::Seed(1234);
		Random::Stream stream;
		stream.Seed(99);
		Random::Stream copy = stream;
		THEN( "numbers come from the stream in a scope, and the main sequence is untouched" ) {
			{
				Random::Scope scope(stream);
				for(int i = 0; i < 10; ++i)
					Random::Real();
			}
			CHECK( Random::Int() == first );
			CHECK( Random::Int() == second );
			CHECK( stream() != copy() );
		}
		THEN( "the same stream gives the same numbers" ) {
			double a = 0.;
			double b = 0.;
			{
				Random::Scope scope(stream);
				a = Random::Real();
			}
			{
				Random::Scope scope(copy);
				b = Random::Real();
			}
			CHECK( a == b );
			CHECK( a >= 0. );
			CHECK( a < 1. );
		}
		THEN( "scopes nest, and an unseeded stream is seeded from the enclosing one" ) {
			Random::Stream inner;
			uint32_t afterInner = 0;
			{
				Random::Scope outer(stream);
				{
					Random::Scope scope(inner);
					Random::Int();
				}
				afterInner = Random::Int();
			}
			CHECK( inner.IsSeeded() );
			// The inner stream's seed was the outer stream's first number.
			copy();
			{
				Random::Scope scope(copy);
				CHECK( Random::Int() == afterInner );
			}
			CHECK( Random::Int() == first );
		}
		THEN( "Int(n) stays below n" ) {
			Random::Scope scope(stream);
			for(int i = 0; i < 1000; ++i)
				CHECK( Random::Int(7) < 7 );
		}
		Random::SetUseStreams(false);
	}
}
// Test code goes here. Preferably, use scenario-driven language making use of the SCENARIO, GIVEN,
// WHEN, and THEN macros. (There will be cases where the more traditional TEST_CASE and SECTION macros
// are better suited to declaration of the public API.)

// When writing assertions, prefer the CHECK and CHECK_FALSE macros when probing the scenario, and prefer
// the REQUIRE / REQUIRE_FALSE macros for fundamental / "validity" assertions. If a CHECK fails, the rest
// of the block's statements will still be evaluated, but a REQUIRE failure will exit the current block.

// #endregion unit tests

// #region benchmarks
#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING
TEST_CASE( "Benchmark Random::Int", "[!benchmark][random]" ) {
	BENCHMARK( "Random::Int()" ) {
		return Random::Int();
	};
	BENCHMARK( "Random::Int(60)" ) {
		return Random::Int(60);
	};
	BENCHMARK( "Random::Int(600)" ) {
		return Random::Int(600);
	};
	BENCHMARK( "Random::Int(10000)" ) {
		return Random::Int(10000);
	};
}
TEST_CASE( "Benchmark Random::Real", "[!benchmark][random]" ) {
	BENCHMARK( "Random::Real" ) {
		return Random::Real();
	};
}
#endif
// #endregion benchmarks



} // test namespace
