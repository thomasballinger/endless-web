/* test_script.cpp
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
#include "../../../source/script/Script.h"

// Include a helper for creating well-formed DataNodes.
#include "datanode-factory.h"

#include <cmath>
#include <string>
#include <vector>

namespace { // test namespace

// #region mock data
// An interface like a host would provide: two inputs, an output, an effect.
script::Interface MakeInterface()
{
	script::Interface interface;
	interface.AddInput("a");
	interface.AddInput("v");
	interface.AddOutput("out");
	interface.AddEffect("ping");
	return interface;
}

// Compile and run a program, returning the output and effects.
struct Result {
	bool valid = false;
	bool ran = false;
	script::Value out;
	uint32_t effects = 0;
};

Result Run(const std::string &text, double a = 0., Point v = Point())
{
	static const script::Interface interface = MakeInterface();
	script::Program program;
	program.Load(AsDataNode(text), interface);
	Result result;
	result.valid = program.IsValid();
	if(!result.valid)
		return result;
	std::vector<script::Value> variables(program.SlotCount());
	variables[0] = script::Value(a);
	variables[1] = script::Value(v);
	variables[2] = script::Value(0.);
	result.ran = program.Run(variables, result.effects);
	result.out = variables[2];
	return result;
}
// #endregion mock data



// #region unit tests
SCENARIO( "Running scripts", "[Script]" ) {
	GIVEN( "arithmetic and nested expressions" ) {
		auto result = Run("program test\n\tset out (+ (* a 2) 1 (- 3))\n", 5.);
		THEN( "they evaluate left to right" ) {
			REQUIRE( result.ran );
			CHECK( result.out.Number() == 8. );
		}
	}
	GIVEN( "vector math" ) {
		Point v(3., 4.);
		auto result = Run("program test\n\tset out (cross (unit v) (* 2 (vec 0 1)))\n", 0., v);
		THEN( "it matches the game's own Point operations exactly" ) {
			REQUIRE( result.ran );
			CHECK( result.out.Number() == v.Unit().Cross(2. * Point(0., 1.)) );
		}
	}
	GIVEN( "an if / elif / else chain" ) {
		std::string text =
R"(program test
	if (< a 0)
		set out -1
	elif (== a 0)
		set out 0
	else
		set out 1
)";
		THEN( "exactly one branch runs" ) {
			CHECK( Run(text, -5.).out.Number() == -1. );
			CHECK( Run(text, 0.).out.Number() == 0. );
			CHECK( Run(text, 7.).out.Number() == 1. );
		}
	}
	GIVEN( "let and set" ) {
		std::string text =
R"(program test
	let x a
	if (> x 1)
		let y (* x x)
		set x y
	set out x
)";
		THEN( "variables can be changed in nested blocks" ) {
			CHECK( Run(text, 3.).out.Number() == 9. );
			CHECK( Run(text, 1.).out.Number() == 1. );
		}
	}
	GIVEN( "effects and stop" ) {
		auto result = Run("program test\n\tping\n\tstop\n\tset out 1\n");
		THEN( "effects are reported and stop ends the program" ) {
			REQUIRE( result.ran );
			CHECK( result.effects == 1u );
			CHECK( result.out.Number() == 0. );
		}
	}
	GIVEN( "\"and\", \"or\" and \"if\" expressions" ) {
		THEN( "they only evaluate what they need" ) {
			// The right-hand side would be a type error if it were evaluated.
			CHECK( Run("program test\n\tset out (if (or true (< v 1)) 1 2)\n").out.Number() == 1. );
			CHECK( Run("program test\n\tset out (if (and false (< v 1)) 1 2)\n").out.Number() == 2. );
		}
	}
}

SCENARIO( "Script errors", "[Script]" ) {
	GIVEN( "programs with compile errors" ) {
		THEN( "they are rejected" ) {
			CHECK_FALSE( Run("program test\n\tset out unknown\n").valid );
			CHECK_FALSE( Run("program test\n\tset a 1\n").valid );
			CHECK_FALSE( Run("program test\n\tset out (frobnicate 1)\n").valid );
			CHECK_FALSE( Run("program test\n\tset out (+ 1\n").valid );
			CHECK_FALSE( Run("program test\n\telse\n\t\tset out 1\n").valid );
			CHECK_FALSE( Run("program test\n\tset out (sqrt 1 2)\n").valid );
			CHECK_FALSE( Run("program test\n\tlet a 1\n").valid );
		}
		THEN( "a variable can't be used outside the block that defines it" ) {
			CHECK_FALSE( Run("program test\n\tif true\n\t\tlet x 1\n\tset out x\n").valid );
		}
	}
	GIVEN( "a program with a type error" ) {
		auto result = Run("program test\n\tset out (+ a v)\n");
		THEN( "it compiles, but stops with an error when run" ) {
			REQUIRE( result.valid );
			CHECK_FALSE( result.ran );
		}
	}
	GIVEN( "a condition that isn't true or false" ) {
		auto result = Run("program test\n\tif a\n\t\tset out 1\n");
		THEN( "it stops with an error" ) {
			REQUIRE( result.valid );
			CHECK_FALSE( result.ran );
		}
	}
}

SCENARIO( "Clamping numbers", "[Script]" ) {
	std::string text = "program test\n\tset out (clamp a -1 2)\n";
	THEN( "values are limited to the range" ) {
		CHECK( Run(text, -5.).out.Number() == -1. );
		CHECK( Run(text, .5).out.Number() == .5 );
		CHECK( Run(text, 7.).out.Number() == 2. );
	}
	THEN( "a value that is not a number stays that way" ) {
		CHECK( std::isnan(Run(text, std::nan("")).out.Number()) );
	}
}

SCENARIO( "Remembering values between runs", "[Script]" ) {
	static const script::Interface interface = MakeInterface();
	auto load = [](const std::string &text) {
		script::Program program;
		program.Load(AsDataNode(text), interface);
		return program;
	};
	auto run = [](const script::Program &program, double a, script::Memory *memory) {
		std::vector<script::Value> variables(program.SlotCount());
		variables[0] = script::Value(a);
		variables[1] = script::Value(Point());
		variables[2] = script::Value(0.);
		uint32_t effects = 0;
		REQUIRE( program.Run(variables, effects, memory) );
		return variables[2].Number();
	};
	GIVEN( "a counter in memory" ) {
		const script::Program program = load(
R"(program test
	memory count a
	set count (+ count 1)
	set out count
)");
		REQUIRE( program.IsValid() );
		THEN( "the initial value is only used on the first run" ) {
			script::Memory memory;
			CHECK( run(program, 10., &memory) == 11. );
			CHECK( run(program, 10., &memory) == 12. );
			CHECK( run(program, 10., &memory) == 13. );
		}
		THEN( "each memory is separate" ) {
			script::Memory first;
			script::Memory second;
			run(program, 0., &first);
			run(program, 0., &first);
			CHECK( run(program, 0., &second) == 1. );
			CHECK( run(program, 0., &first) == 3. );
		}
		THEN( "without a memory, it starts over every run" ) {
			CHECK( run(program, 5., nullptr) == 6. );
			CHECK( run(program, 5., nullptr) == 6. );
		}
		THEN( "clearing a memory starts it over" ) {
			script::Memory memory;
			run(program, 0., &memory);
			memory.Clear();
			CHECK( run(program, 0., &memory) == 1. );
		}
	}
	GIVEN( "a memory declared inside a block" ) {
		const script::Program program = load("program test\n\tif true\n\t\tmemory x 0\n");
		THEN( "the program is invalid" ) {
			CHECK( !program.IsValid() );
		}
	}
}
// #endregion unit tests



} // test namespace
