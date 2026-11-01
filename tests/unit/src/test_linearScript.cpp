/* test_linearScript.cpp
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

#include <string>
#include <vector>

namespace { // test namespace

// #region mock data
// An input, an output and an effect, and two routines in the prelude: one that
// reads the input, and one that triggers the effect and waits.
script::Interface MakeInterface()
{
	script::Interface interface;
	interface.AddInput("a");
	interface.AddOutput("out");
	interface.AddEffect("ping");
	interface.SetPrelude("routine copy_a()\n\tout = a\n"
		"routine ping_after(steps)\n\twait steps\n\tping\n");
	return interface;
}

const script::Interface &GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}

// Run a program for some steps, as a host would: each step, the input is
// given and the output starts at 0. Returns the output after each step.
std::vector<double> RunSteps(script::Program &program, int count, std::vector<double> a = {},
	std::vector<uint32_t> *effects = nullptr)
{
	script::Memory memory;
	std::vector<double> outputs;
	for(int i = 0; i < count; ++i)
	{
		std::vector<script::Value> variables(program.SlotCount());
		variables[0] = script::Value(i < static_cast<int>(a.size()) ? a[i] : 0.);
		variables[1] = script::Value(0.);
		uint32_t triggered = 0;
		if(!program.Run(variables, triggered, &memory))
			break;
		outputs.push_back(variables[1].Number());
		if(effects)
			effects->push_back(triggered);
	}
	return outputs;
}

script::Program Compile(const std::string &text)
{
	script::Program program;
	program.Load(AsDataNode(text), GetInterface(), false);
	return program;
}
// #endregion mock data



// #region unit tests
SCENARIO( "Linear scripts wait, and carry on where they waited", "[Script][Linear]" ) {
	GIVEN( "a script that sets its output, waits, and sets it again" ) {
		script::Program program = Compile("program test\n\tout = 1\n\twait 2\n\tout = 2\n\twait\n\tout = 3\n");
		REQUIRE( program.IsValid() );
		THEN( "it is linear" ) {
			CHECK( program.IsLinear() );
		}
		THEN( "each step carries on from the last, and its output is kept until it changes" ) {
			CHECK( RunSteps(program, 6) == std::vector<double>{1., 1., 2., 3., 3., 3.} );
		}
	}
	GIVEN( "a script with no waits" ) {
		script::Program program = Compile("program test\n\tout = a + 1\n");
		THEN( "it is not linear, and runs from the start every step" ) {
			CHECK_FALSE( program.IsLinear() );
			CHECK( RunSteps(program, 3, {1., 5., 2.}) == std::vector<double>{2., 6., 3.} );
		}
	}
	GIVEN( "a while loop that waits each time around" ) {
		script::Program program = Compile("program test\n\tlet n = 0\n\twhile n < 3\n\t\tn = n + 1\n\t\tout = n\n"
			"\t\twait\n\tout = 10\n");
		REQUIRE( program.IsValid() );
		THEN( "its variables are kept between steps, and it leaves the loop when the condition fails" ) {
			CHECK( RunSteps(program, 5) == std::vector<double>{1., 2., 3., 10., 10.} );
		}
	}
	GIVEN( "a routine that waits, run in one branch of an \"if\"" ) {
		script::Program program = Compile("program test\n\troutine pulse(value, steps)\n\t\tout = value\n"
			"\t\twait steps\n\t\tout = 0\n\tif a > 0\n\t\tpulse(5, 2)\n\telse\n\t\tout = 9\n\tout = out + 7\n");
		REQUIRE( program.IsValid() );
		THEN( "the branch is chosen once, and the routine carries on even if the input changes" ) {
			CHECK( RunSteps(program, 4, {1., -1., -1., -1.}) == std::vector<double>{5., 5., 7., 7.} );
		}
		THEN( "the other branch doesn't wait" ) {
			CHECK( RunSteps(program, 2, {-1., 1.}) == std::vector<double>{16., 16.} );
		}
	}
	GIVEN( "a loop that runs forever" ) {
		script::Program program = Compile("program test\n\tloop\n\t\tout = 1\n\t\twait\n\t\tout = -1\n\t\twait\n");
		REQUIRE( program.IsValid() );
		THEN( "it alternates, step after step" ) {
			CHECK( RunSteps(program, 5) == std::vector<double>{1., -1., 1., -1., 1.} );
		}
	}
	GIVEN( "a script that stops partway" ) {
		script::Program program = Compile("program test\n\tout = 4\n\twait\n\tstop\n\tout = 5\n");
		THEN( "it finishes there, keeping its output" ) {
			CHECK( RunSteps(program, 4) == std::vector<double>{4., 4., 4., 4.} );
		}
	}
	GIVEN( "a loop that never waits" ) {
		script::Program program = Compile("program test\n\tlet n = 0\n\twhile true\n\t\tn = n + 1\n\twait\n");
		THEN( "the run fails, rather than hanging" ) {
			CHECK( RunSteps(program, 2).empty() );
		}
	}
}

SCENARIO( "Routines from the interface's prelude", "[Script][Linear]" ) {
	GIVEN( "a script that doesn't call the prelude's routines" ) {
		script::Program program = Compile("program test\n\tout = 1\n");
		THEN( "it doesn't read or trigger what they do" ) {
			REQUIRE( program.IsValid() );
			CHECK_FALSE( program.Reads(0) );
			CHECK_FALSE( program.Triggers(0) );
		}
	}
	GIVEN( "a script that calls them" ) {
		script::Program program = Compile("program test\n\tcopy_a()\n\tping_after(2)\n\tout = 8\n");
		REQUIRE( program.IsValid() );
		THEN( "it reads and triggers what they do, and is linear if they wait" ) {
			CHECK( program.Reads(0) );
			CHECK( program.Triggers(0) );
			CHECK( program.IsLinear() );
		}
		THEN( "they run as if written in the script" ) {
			std::vector<uint32_t> effects;
			CHECK( RunSteps(program, 4, {3., 0., 0., 0.}, &effects) == std::vector<double>{3., 3., 8., 8.} );
			CHECK( effects == std::vector<uint32_t>{0u, 0u, 1u, 0u} );
		}
	}
}

SCENARIO( "Where a script can't wait", "[Script][Linear]" ) {
	THEN( "a function can't wait or run a routine" ) {
		CHECK_FALSE( Compile("program test\n\tfunction f(x)\n\t\twait\n\t\treturn x\n\tout = f(1)\n").IsValid() );
		CHECK_FALSE( Compile("program test\n\tfunction f(x)\n\t\tcopy_a()\n\t\treturn x\n\tout = f(1)\n").IsValid() );
	}
	THEN( "a routine can't run a routine defined below it" ) {
		CHECK_FALSE( Compile("program test\n\troutine r()\n\t\ts()\n\troutine s()\n\t\twait\n\tr()\n").IsValid() );
	}
	THEN( "a routine must be called with the right number of arguments" ) {
		CHECK_FALSE( Compile("program test\n\tping_after(1, 2)\n").IsValid() );
	}
}

SCENARIO( "Tracing a linear script", "[Script][Linear]" ) {
	GIVEN( "a script waiting in a routine from the prelude" ) {
		script::Program program = Compile("program test\n\tout = 1\n\tping_after(3)\n");
		REQUIRE( program.IsValid() );
		script::Memory memory;
		script::Trace trace;
		std::vector<script::Value> variables(program.SlotCount());
		uint32_t effects = 0;
		program.Run(variables, effects, &memory, &trace);
		program.Run(variables, effects, &memory, &trace);
		THEN( "the trace shows it waiting on the line that called the routine" ) {
			REQUIRE( !trace.steps.empty() );
			CHECK( trace.steps.back().kind == script::Trace::Step::Kind::WAIT );
			CHECK( trace.steps.back().line == 3 );
			CHECK( trace.steps.back().value.Number() == 2. );
		}
	}
}
SCENARIO( "Reading guarded inputs after \"&&\" and \"?\"", "[Script][Linear]" ) {
	// "dist" only means something while "seen" is true.
	script::Interface interface;
	const int seen = interface.AddInput("seen");
	const int dist = interface.AddInput("dist");
	interface.AddOutput("out");
	interface.SetGuard(dist, seen);
	auto valid = [&interface](const std::string &text) {
		script::Program program;
		program.Load(AsDataNode(text), interface, false);
		return program.IsValid();
	};
	THEN( "it can be read after the guard in an \"and\", or a choice on it" ) {
		CHECK( valid("program test\n\tout = seen && dist > 5 ? 1 : 0\n") );
		CHECK( valid("program test\n\tout = seen ? dist : 0\n") );
		CHECK( valid("program test\n\twhile seen && dist > 5\n\t\tout = dist\n\t\twait\n") );
	}
	THEN( "but not before it, after \"or\", or in the other branch of a choice" ) {
		CHECK_FALSE( valid("program test\n\tout = dist > 5 && seen ? 1 : 0\n") );
		CHECK_FALSE( valid("program test\n\tout = seen || dist > 5 ? 1 : 0\n") );
		CHECK_FALSE( valid("program test\n\tout = seen ? 0 : dist\n") );
	}
}
// #endregion unit tests



} // test namespace
