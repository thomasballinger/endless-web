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

// Include the game's random numbers, which scripts can draw from.
#include "../../../source/Random.h"

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
	GIVEN( "\"*\", \"min\" and \"max\" with more than two arguments" ) {
		THEN( "they work from left to right, exactly like nested calls" ) {
			const double a = .1;
			CHECK( Run("program test\n\tset out (* a 3 7 .3)\n", a).out.Number() == a * 3. * 7. * .3 );
			CHECK( Run("program test\n\tset out (min 4 a -2 9)\n", a).out.Number() == -2. );
			CHECK( Run("program test\n\tset out (max 4 a -2 9)\n", a).out.Number() == 9. );
			const Point v(1.5, -2.);
			auto vector = Run("program test\n\tset out (x (* 2 v 3))\n", 0., v);
			REQUIRE( vector.ran );
			CHECK( vector.out.Number() == (2. * v * 3.).X() );
			CHECK_FALSE( Run("program test\n\tset out (x (* v 2 v))\n", 0., v).ran );
			CHECK_FALSE( Run("program test\n\tset out (min 1 2 v)\n", 0., v).ran );
		}
	}
	GIVEN( "functions defined by the script" ) {
		const std::string text =
R"(program test
	function clamp-to-a x
		if (> x a)
			return a
		let negative (- a)
		if (< x negative)
			return negative
		return x
	function twice-clamped x y
		return (* 2 (+ (clamp-to-a x) (clamp-to-a y)))
	set out (twice-clamped (clamp-to-a 100) -1)
)";
		THEN( "they can use their parameters, inputs, their own variables and earlier functions" ) {
			auto result = Run(text, 3.);
			REQUIRE( result.valid );
			REQUIRE( result.ran );
			CHECK( result.out.Number() == 2. * (3. + -1.) );
		}
		THEN( "a call as an argument to the same function works" ) {
			auto result = Run("program test\n\tfunction add-one x\n\t\treturn (+ x 1)\n"
				"\tset out (add-one (add-one (add-one a)))\n", 1.);
			REQUIRE( result.ran );
			CHECK( result.out.Number() == 4. );
		}
		THEN( "a function that doesn't return a value stops the script" ) {
			auto result = Run("program test\n\tfunction maybe x\n\t\tif (> x 0)\n\t\t\treturn x\n"
				"\tset out (maybe a)\n", -1.);
			REQUIRE( result.valid );
			CHECK_FALSE( result.ran );
		}
	}
	GIVEN( "functions that break the rules" ) {
		THEN( "they are rejected" ) {
			// Calling itself, or a function defined below.
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\treturn (f x)\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\treturn (g x)\n"
				"\tfunction g x\n\t\treturn x\n").valid);
			// Changing an output, using memory or effects, or stopping.
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\tset out x\n\t\treturn x\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\tmemory m 0\n\t\treturn x\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\tping\n\t\treturn x\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\tstop\n").valid );
			// Seeing the script's variables, rather than only its parameters.
			CHECK_FALSE( Run("program test\n\tlet y 1\n\tfunction f x\n\t\treturn (+ x y)\n").valid );
			// Defined inside a block, named like a built-in function, or called wrongly.
			CHECK_FALSE( Run("program test\n\tif true\n\t\tfunction f x\n\t\t\treturn x\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction sqrt x\n\t\treturn x\n").valid );
			CHECK_FALSE( Run("program test\n\tfunction f x\n\t\treturn x\n\tset out (f 1 2)\n").valid );
			// "return" outside of a function.
			CHECK_FALSE( Run("program test\n\treturn 1\n").valid );
		}
	}
	GIVEN( "random numbers" ) {
		THEN( "they come from the game's random numbers, in order" ) {
			Random::Seed(5);
			const double first = Random::Real();
			const uint32_t second = Random::Int(10);
			Random::Seed(5);
			auto result = Run("program test\n\tlet r (random)\n\tset out (+ r (* 100 (random-int 10)))\n");
			REQUIRE( result.ran );
			CHECK( result.out.Number() == first + 100. * second );
		}
		THEN( "a tape notes them, and plays them back to another run" ) {
			const std::string program = "program test\n\tlet r (random)\n\tset out (+ r (* 100 (random-int 10)))\n";
			script::RandomTape tape;
			Random::Seed(5);
			Result recorded;
			{
				script::RandomTape::Use use(&tape);
				recorded = Run(program);
			}
			// Playing back draws nothing from the game's random numbers.
			Random::Seed(6);
			tape.Play();
			Result played;
			{
				script::RandomTape::Use use(&tape);
				played = Run(program);
			}
			CHECK( played.out.Number() == recorded.out.Number() );
			CHECK( tape.Matches() );
			CHECK( tape.IsFinished() );
			Random::Seed(5);
			CHECK( Run(program).out.Number() == recorded.out.Number() );
		}
		THEN( "a tape notices a run that draws differently" ) {
			script::RandomTape tape;
			{
				script::RandomTape::Use use(&tape);
				Run("program test\n\tset out (random-int 10)\n");
			}
			tape.Play();
			{
				script::RandomTape::Use use(&tape);
				Run("program test\n\tset out (random-int 20)\n");
			}
			CHECK_FALSE( tape.Matches() );
			tape.Play();
			{
				script::RandomTape::Use use(&tape);
				Run("program test\n\tset out 1\n");
			}
			CHECK( tape.Matches() );
			CHECK_FALSE( tape.IsFinished() );
		}
		THEN( "random-int needs a whole number of at least 1" ) {
			CHECK( Run("program test\n\tset out (random-int 1)\n").out.Number() == 0. );
			CHECK_FALSE( Run("program test\n\tset out (random-int 0)\n").ran );
			CHECK_FALSE( Run("program test\n\tset out (random-int 2.5)\n").ran );
			CHECK_FALSE( Run("program test\n\tset out (random-int v)\n").ran );
			CHECK_FALSE( Run("program test\n\tset out (random 1)\n").valid );
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
	GIVEN( "a variable with the same name as an input" ) {
		auto result = Run("program test\n\tlet a (* a 10)\n\tset out (+ a 1)\n", 2.);
		THEN( "it hides the input, so new inputs can't break old scripts" ) {
			REQUIRE( result.valid );
			REQUIRE( result.ran );
			CHECK( result.out.Number() == 21. );
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
			// A script's variable may not hide an output or a constant.
			CHECK_FALSE( Run("program test\n\tlet out 1\n").valid );
			CHECK_FALSE( Run("program test\n\tlet pi 3\n").valid );
			CHECK_FALSE( Run("program test\n\tlet x 1\n\tlet x 2\n").valid );
			// A variable whose definition has an error is still invalid to use.
			CHECK_FALSE( Run("program test\n\tlet x (sqrt 1 2)\n\tset out x\n").valid );
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
	GIVEN( "a program that is edited and loaded again" ) {
		const script::Program before = load("program test\n\tmemory count a\n\tset count (+ count 1)\n\tset out count\n");
		// The edited version keeps another value, and counts differently.
		const script::Program after = load("program test\n\tmemory count a\n\tmemory where v\n"
			"\tset count (+ count 2)\n\tset out count\n");
		REQUIRE( before.IsValid() );
		REQUIRE( after.IsValid() );
		THEN( "memory kept for the old version starts over" ) {
			script::Memory memory;
			run(before, 0., &memory);
			run(before, 0., &memory);
			CHECK( run(after, 10., &memory) == 12. );
			CHECK( run(after, 10., &memory) == 14. );
		}
	}
	GIVEN( "a memory declared inside a block" ) {
		const script::Program program = load("program test\n\tif true\n\t\tmemory x 0\n");
		THEN( "the program is invalid" ) {
			CHECK( !program.IsValid() );
		}
	}
}
SCENARIO( "Tracing a run and reporting errors", "[Script]" ) {
	static const script::Interface interface = MakeInterface();
	GIVEN( "a program with a function, a branch, and an effect" ) {
		script::Program program;
		program.Load(AsDataNode(
R"(program test
	function double x
		return (* x 2)
	let y (double a)
	if (> y 5)
		set out y
		ping
	else
		set out 0
)"), interface);
		REQUIRE( program.IsValid() );
		std::vector<script::Value> variables(program.SlotCount());
		variables[0] = script::Value(4.);
		variables[1] = script::Value(Point());
		variables[2] = script::Value(0.);
		uint32_t effects = 0;
		script::Trace trace;
		REQUIRE( program.Run(variables, effects, nullptr, &trace) );
		THEN( "the trace has each statement that ran, with its line and value" ) {
			using Kind = script::Trace::Step::Kind;
			REQUIRE( trace.steps.size() == 5 );
			// Lines count from the "program" line, which is line 1.
			CHECK( trace.steps[0].kind == Kind::RETURN );
			CHECK( trace.steps[0].line == 3 );
			CHECK( trace.steps[0].depth == 1 );
			CHECK( trace.steps[0].value.Number() == 8. );
			CHECK( trace.steps[1].kind == Kind::ASSIGN );
			CHECK( trace.steps[1].line == 4 );
			CHECK( trace.steps[1].name == "y" );
			CHECK( trace.steps[1].depth == 0 );
			CHECK( trace.steps[2].kind == Kind::CONDITION );
			CHECK( trace.steps[2].line == 5 );
			CHECK( trace.steps[2].value.Boolean() );
			CHECK( trace.steps[3].name == "out" );
			CHECK( trace.steps[3].value.ToString() == "8" );
			CHECK( trace.steps[4].kind == Kind::EFFECT );
			CHECK( trace.steps[4].name == "ping" );
			CHECK( trace.error.empty() );
		}
		THEN( "running without a trace still works" ) {
			CHECK( program.Run(variables, effects) );
		}
	}
	GIVEN( "a program that fails while running" ) {
		script::Program program;
		program.Load(AsDataNode("program test\n\tlet x 1\n\tset out (+ a v)\n"), interface);
		REQUIRE( program.IsValid() );
		std::vector<script::Value> variables(program.SlotCount());
		variables[0] = script::Value(1.);
		variables[1] = script::Value(Point(1., 2.));
		variables[2] = script::Value(0.);
		uint32_t effects = 0;
		script::Trace trace;
		THEN( "the trace says where and why" ) {
			CHECK_FALSE( program.Run(variables, effects, nullptr, &trace) );
			CHECK( trace.errorLine == 3 );
			CHECK( trace.error.find("+") != std::string::npos );
		}
	}
	GIVEN( "a program with mistakes on two lines" ) {
		script::Program program;
		program.Load(AsDataNode("program test\n\tlet x (frobnicate 1)\n\tset out x\n\tset nothing 2\n"),
			interface, false);
		THEN( "each mistake is reported with its line" ) {
			CHECK_FALSE( program.IsValid() );
			REQUIRE( program.Errors().size() == 2 );
			CHECK( program.Errors()[0].line == 2 );
			CHECK( program.Errors()[0].message == "Unknown function \"frobnicate\"" );
			CHECK( program.Errors()[1].line == 4 );
		}
	}
	THEN( "values describe themselves" ) {
		CHECK( script::Value(1.5).ToString() == "1.5" );
		CHECK( script::Value(true).ToString() == "true" );
		CHECK( script::Value(Point(3., -0.25)).ToString() == "(3, -0.25)" );
	}
}

SCENARIO( "Looping over lists of records", "[Script]" ) {
	// An interface with a list of things, each with a size and a position.
	static const script::Interface interface = [] {
		script::Interface result;
		result.AddInput("limit");
		result.AddList("things", {"size", "where"});
		result.AddOutput("out");
		return result;
	}();
	script::RecordList things(2);
	for(double size : {3., 8., 5., 9.})
	{
		things.Add();
		things.SetField(0, script::Value(size));
		things.SetField(1, script::Value(Point(size, -size)));
	}
	auto run = [&things](const std::string &text, double limit, bool &valid) {
		script::Program program;
		program.Load(AsDataNode(text), interface);
		valid = program.IsValid();
		std::vector<script::Value> variables(program.SlotCount());
		variables[0] = script::Value(limit);
		variables[1] = script::Value(things);
		variables[2] = script::Value(-1.);
		uint32_t effects = 0;
		if(valid && !program.Run(variables, effects))
			return std::nan("");
		return variables[2].Number();
	};
	bool valid = false;
	GIVEN( "a loop that picks the largest thing under a limit" ) {
		const std::string text =
R"(program test
	let best 0
	for t in things
		if (and (< t.size limit) (> t.size best))
			set best t.size
			set out t.index
)";
		THEN( "it sees every record's fields and index, in order" ) {
			CHECK( run(text, 6., valid) == 2. );
			REQUIRE( valid );
			CHECK( run(text, 100., valid) == 3. );
			CHECK( run(text, 1., valid) == -1. );
		}
	}
	GIVEN( "a function that loops" ) {
		THEN( "it can return from inside the loop" ) {
			CHECK( run("program test\n\tfunction first-over x\n\t\tfor t in things\n\t\t\tif (> t.size x)\n"
				"\t\t\t\treturn (y t.where)\n\t\treturn 0\n\tset out (first-over limit)\n", 6., valid) == -8.);
			CHECK( valid );
		}
	}
	GIVEN( "lists and records used other than through their fields" ) {
		THEN( "the script is rejected" ) {
			run("program test\n\tlet copy things\n", 0., valid);
			CHECK_FALSE( valid );
			run("program test\n\tfor t in things\n\t\tlet copy t\n", 0., valid);
			CHECK_FALSE( valid );
			run("program test\n\tfor t in things\n\t\tset out t.color\n", 0., valid);
			CHECK_FALSE( valid );
			run("program test\n\tfor t in limit\n\t\tset out 1\n", 0., valid);
			CHECK_FALSE( valid );
			run("program test\n\tfor t in things\n\t\tset out 1\n\tset out t.size\n", 0., valid);
			CHECK_FALSE( valid );
		}
	}
}
SCENARIO( "Infix scripts", "[Script]" ) {
	GIVEN( "infix scripts and the same programs in prefix form" ) {
		// Each pair must compile to the same program, so give the same result
		// for any input.
		const std::vector<std::pair<std::string, std::string>> pairs = {
			{"out = a * 2 + 1 - 3", "set out (- (+ (* a 2) 1) 3)"},
			{"out = -(a - 1) * 2", "set out (* (- (- a 1)) 2)"},
			{"out = a / 2 / 4", "set out (/ (/ a 2) 4)"},
			{"out = a > 3 && !(a > 10) ? 1 : 2", "set out (if (and (> a 3) (not (> a 10))) 1 2)"},
			{"out = a < 0 || a >= 7 ? a : -a", "set out (if (or (< a 0) (>= a 7)) a (- a))"},
			{"out = v.x + v.y * 2", "set out (+ (x v) (* (y v) 2))"},
			{"out = length(v) + copysign(1, a)", "set out (+ (length v) (copysign 1 a))"},
			{"out = to_deg * asin(cross(unit(v), vec(0, -1)))",
				"set out (* to-deg (asin (cross (unit v) (vec 0 -1))))"},
			{"out = a == 5 ? 1e3 : 2.5e-1", "set out (if (== a 5) 1e3 2.5e-1)"},
			{"out = max(a, 1, 2) - min(a, 3)", "set out (- (max (max a 1) 2) (min a 3))"},
		};
		for(const auto &[infix, prefix] : pairs)
			for(double a : {-4., 0., 5., 8., 12.})
			{
				auto x = Run("program test\n\t" + infix + "\n", a, Point(3., 4.));
				auto y = Run("program test\n\t" + prefix + "\n", a, Point(3., 4.));
				INFO( infix );
				REQUIRE( x.ran );
				REQUIRE( y.ran );
				CHECK( x.out.Number() == y.out.Number() );
			}
	}
	GIVEN( "infix statements" ) {
		THEN( "let, set, if, else if, else and functions work" ) {
			CHECK( Run("program test\n\tlet my_value = a + 1\n\tout = my_value\n", 5.).out.Number() == 6. );
			const std::string chain = "program test\n\tif a > 10\n\t\tout = 1\n\telse if a > 5\n\t\tout = 2\n"
				"\telse\n\t\tout = 3\n";
			CHECK( Run(chain, 11.).out.Number() == 1. );
			CHECK( Run(chain, 6.).out.Number() == 2. );
			CHECK( Run(chain, 1.).out.Number() == 3. );
			CHECK( Run("program test\n\tfunction twice(x, y)\n\t\treturn x * 2 + y\n\tout = twice(a, 1)\n", 5.)
				.out.Number() == 11. );
			CHECK( Run("program test\n\tif a > 1\n\t\tping\n", 5.).effects == 1 );
		}
		THEN( "mistakes are rejected" ) {
			CHECK_FALSE( Run("program test\n\tout = a +\n").valid );
			CHECK_FALSE( Run("program test\n\tlet = 3\n").valid );
			CHECK_FALSE( Run("program test\n\tout = (a + 1\n").valid );
			CHECK_FALSE( Run("program test\n\tout = a $ 1\n").valid );
			CHECK_FALSE( Run("program test\n\ta = 1\n").valid );
			CHECK_FALSE( Run("program test\n\tout = nothing_here\n").valid );
		}
	}
	GIVEN( "an input that is only known while another is true" ) {
		script::Interface interface;
		const int lock = interface.AddInput("has-lock");
		interface.SetGuard(interface.AddInput("target-position"), lock);
		interface.AddAlias("target", lock);
		interface.AddOutput("out");
		auto valid = [&interface](const std::string &text) {
			script::Program program;
			program.Load(AsDataNode(text), interface, false);
			return program.IsValid();
		};
		THEN( "infix scripts may only read it where that is certain" ) {
			CHECK_FALSE( valid("program test\n\tout = target.position.x\n") );
			CHECK_FALSE( valid("program test\n\tif !target\n\t\tout = target.position.x\n") );
			CHECK_FALSE( valid("program test\n\tif target\n\t\tout = 1\n\telse\n\t\tout = target_position.x\n") );
			CHECK( valid("program test\n\tif target\n\t\tout = target.position.x\n") );
			CHECK( valid("program test\n\tif target && out > 1\n\t\tout = target_position.y\n") );
			CHECK( valid("program test\n\tif !target\n\t\tstop\n\tout = target.position.x\n") );
			CHECK_FALSE( valid("program test\n\tfunction f(z)\n\t\treturn target.position.x\n\tif target\n"
				"\t\tout = f(1)\n") );
		}
		THEN( "prefix scripts are not held to it" ) {
			CHECK( valid("program test\n\tset out (x target-position)\n") );
		}
	}
	GIVEN( "a traced infix script" ) {
		script::Program program;
		program.Load(AsDataNode("program test\n\tlet my_value = a + 1\n\tout = my_value\n"), MakeInterface());
		std::vector<script::Value> variables(program.SlotCount());
		uint32_t effects = 0;
		script::Trace trace;
		REQUIRE( program.Run(variables, effects, nullptr, &trace) );
		THEN( "names are shown as they were written" ) {
			REQUIRE( trace.steps.size() == 2 );
			CHECK( trace.steps[0].name == "my_value" );
		}
	}
}
// #endregion unit tests



} // test namespace
