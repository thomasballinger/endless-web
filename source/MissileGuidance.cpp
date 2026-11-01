/* MissileGuidance.cpp
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

#include "MissileGuidance.h"

#include "DataFile.h"
#include "DataNode.h"
#include "GameData.h"
#include "pi.h"
#include "Weapon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <vector>

using namespace std;

namespace {
	// The guidance programs the player has written, by weapon, and their text.
	map<const Weapon *, shared_ptr<const MissileGuidance>> &PlayerPrograms()
	{
		static map<const Weapon *, shared_ptr<const MissileGuidance>> programs;
		return programs;
	}

	bool useFactionGuidance = false;

	// Compile the first guidance node in the given text.
	shared_ptr<MissileGuidance> Compile(const string &text)
	{
		istringstream in(text);
		DataFile file(in);
		for(const DataNode &node : file)
			if(node.Token(0) == "guidance" && node.Size() >= 2)
			{
				auto guidance = make_shared<MissileGuidance>();
				guidance->Load(node, node.Token(1), false);
				return guidance;
			}
		return nullptr;
	}

	map<const Weapon *, string> &PlayerProgramTexts()
	{
		static map<const Weapon *, string> texts;
		return texts;
	}

	// Routines any guidance script can run. Each takes some steps, waiting
	// (see "wait") until it is done; a script that runs one becomes linear,
	// carrying on from where it waited on each step, rather than deciding
	// afresh every step. That is simple to write, but it can't react: a turn
	// toward where the target is goes on toward where it was.
	const char *const PRELUDE =
		// Turn to face a direction, fixed when the turn starts.
		"routine turn_to(goal)\n"
		"\tlet aim = unit(goal)\n"
		"\tlet tries = 360 / max_turn + 2\n"
		"\twhile abs(bearing(aim)) > .5 && tries > 0\n"
		"\t\tturn = turn_toward(aim)\n"
		"\t\ttries = tries - 1\n"
		"\t\twait\n"
		"\tturn = 0\n"
		// Turn by some degrees (positive is clockwise).
		"routine turn_by(degrees)\n"
		"\tlet a = degrees * to_rad\n"
		"\tturn_to(vec(facing.x * cos(a) - facing.y * sin(a), facing.x * sin(a) + facing.y * cos(a)))\n"
		// Fly straight for some steps.
		"routine fly(steps)\n"
		"\tturn = 0\n"
		"\twait steps\n"
		// Thrust at full power for some steps (needs throttle control).
		"routine burn(steps)\n"
		"\tthrust = acceleration\n"
		"\twait steps\n"
		// Cut the engine for some steps, then thrust again.
		"routine coast(steps)\n"
		"\tthrust = 0\n"
		"\twait steps\n"
		"\tthrust = acceleration\n"
		// Home on the target, deciding afresh every step, for some steps.
		"routine track(steps)\n"
		"\tlet left = steps\n"
		"\twhile left > 0\n"
		"\t\tif target\n"
		"\t\t\tturn = turn_toward(target.direction)\n"
		"\t\telse\n"
		"\t\t\tturn = 0\n"
		"\t\tleft = left - 1\n"
		"\t\twait\n"
		"\tturn = 0\n";

	// The input of the script running on this thread, for its functions.
	const MissileGuidance::Input *&CurrentInput()
	{
		thread_local const MissileGuidance::Input *input = nullptr;
		return input;
	}

	// These do exactly what the default scripts did, in the same order, so
	// that scripts using them steer exactly as those did.
	double Bearing(const Point &facing, const Point &direction)
	{
		return TO_DEG * asin(facing.Cross(direction));
	}

	double LimitTurn(double turn, double maxTurn)
	{
		return fabs(turn) > maxTurn ? copysign(maxTurn, turn) : turn;
	}

	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as MissileGuidance::Variable.
		interface.AddInput("position");
		interface.AddInput("velocity");
		interface.AddInput("facing");
		interface.AddInput("has-lock");
		interface.AddInput("target-position");
		interface.AddInput("target-velocity");
		interface.AddInput("max-turn");
		interface.AddInput("acceleration");
		interface.AddInput("drag");
		interface.AddInput("age");
		interface.AddInput("lifetime-left");
		interface.AddInput("target-facing");
		interface.AddInput("top-speed");
		interface.AddInput("target-offset");
		interface.AddInput("target-distance");
		interface.AddInput("target-direction");
		interface.AddInput("target-bearing");
		interface.AddInput("target-steps-away");
		interface.AddInput("target-intercept");
		interface.AddInput("target-intercept-steps");
		interface.AddOutput("turn");
		interface.AddOutput("thrust");
		interface.AddEffect("drop-target");
		// In infix scripts, what the seeker sees of its target is
		// "target.position" and so on, which can only be read inside
		// "if target" (that is, while it has a lock).
		const int hasLock = interface.Find("has-lock");
		interface.AddAlias("target", hasLock);
		for(const char *input : {"target-position", "target-velocity", "target-facing", "target-offset",
				"target-distance", "target-direction", "target-bearing", "target-steps-away", "target-intercept",
				"target-intercept-steps"})
			interface.SetGuard(interface.Find(input), hasLock);

		// "bearing(direction)": the degrees to turn to face the given direction.
		// "turn_toward(direction)": the same, limited to the most it can turn.
		auto direction = [](const script::Value *arguments, Point &result, string &error) {
			if(arguments[0].GetType() != script::Value::Type::VECTOR || !CurrentInput())
			{
				error = "wrong type of argument: it takes a direction (a vector)";
				return false;
			}
			// Any vector will do: only its direction matters. (A unit vector is
			// used as it is, so that the factory scripts' turns don't change.)
			result = arguments[0].Vector();
			const double length = result.Length();
			if(length && fabs(length - 1.) > 1e-9)
				result /= length;
			return true;
		};
		interface.AddFunction("bearing", 1, [direction](const script::Value *arguments, script::Value &result,
				string &error) {
			Point toward;
			if(!direction(arguments, toward, error))
				return false;
			result = script::Value(Bearing(CurrentInput()->facing, toward));
			return true;
		});
		interface.AddFunction("turn-toward", 1, [direction](const script::Value *arguments, script::Value &result,
				string &error) {
			Point toward;
			if(!direction(arguments, toward, error))
				return false;
			result = script::Value(LimitTurn(Bearing(CurrentInput()->facing, toward), CurrentInput()->maxTurn));
			return true;
		});
		// Routines for linear scripts: each one does something over several
		// steps, deciding how when it starts (see PRELUDE).
		interface.SetPrelude(PRELUDE);
		return interface;
	}
}



const map<string, MissileGuidance::Doc> &MissileGuidance::Documentation()
{
	static const map<string, Doc> DOCS = {
		{"position", {"Where this projectile is.", ""}},
		{"velocity", {"How far it moves each step, and which way.", ""}},
		{"facing", {"The unit vector it faces (which way it thrusts).", ""}},
		{"target", {"Whether its seeker sees a target. Only inside \"if target\" can a script use what the seeker"
			" sees of it (target.position and the rest).", "the seeker has a lock"}},
		{"target.position", {"Where the target is.", ""}},
		{"target.velocity", {"How the target is moving. Needs a \"leading\" seeker.", ""}},
		{"target.facing", {"The unit vector the target faces. Needs \"optical tracking\".", ""}},
		{"max_turn", {"The most degrees it can turn in one step.", "the weapon's \"turn\""}},
		{"acceleration", {"How much it speeds up each step at full thrust.", "the weapon's \"acceleration\""}},
		{"drag", {"The fraction of its speed it loses each step.", "the weapon's \"drag\""}},
		{"top_speed", {"How fast it can go, when thrust and drag balance.",
			"drag != 0 ? acceleration / drag : length(velocity)"}},
		{"age", {"Steps since it was fired.", ""}},
		{"lifetime_left", {"Steps until it expires.", ""}},
		{"target.offset", {"From this projectile to the target.", "target.position - position"}},
		{"target.distance", {"How far away the target is.", "length(target.offset)"}},
		{"target.direction", {"The unit vector toward the target.", "unit(target.offset)"}},
		{"target.bearing", {"The degrees to turn to face the target: positive is clockwise. From -90 to 90, so a"
			" target right behind is 0.", "bearing(target.direction)"}},
		{"target.steps_away", {"How many steps it would take to get to where the target is, at top speed.",
			"target.distance / top_speed"}},
		{"target.intercept", {"The unit vector toward where to meet the target: if it is coming closer, where the"
			" two have the same speed across the line between them; otherwise where it will be by the time this"
			" gets to where it is now. Needs \"leading\".",
			"unit(target.offset + target.steps_away * target.velocity), or head on, closing_speed * direction"
			" + normal_speed * normal"}},
		{"target.intercept_steps", {"How many steps it would take to get to where to meet the target.",
			"length(where to meet) / top_speed"}},
		{"turn", {"Output: degrees to turn this step (positive is clockwise). Starts at max_turn; no more than"
			" max_turn either way is used.", ""}},
		{"thrust", {"Output: how much to speed up this step. Starts at acceleration; from 0 up to acceleration"
			" is used. Setting it needs \"throttle control\".", ""}},
		{"bearing", {"bearing(direction): the degrees to turn to face a direction, given as any vector pointing"
			" that way (positive is clockwise, from -90 to 90).", "to_deg * asin(cross(facing, unit(direction)))"}},
		{"turn_toward", {"turn_toward(direction): the degrees to turn this step toward a direction, no more than"
			" max_turn.", "abs(bearing(d)) > max_turn ? copysign(max_turn, bearing(d)) : bearing(d)"}},
		{"drop_target", {"Forget the target (the seeker may pick it up again).", ""}},
		{"stop", {"End the script for this step (or, in a linear script, for good).", ""}},
		{"wait", {"wait n: stop here for n steps (1 if not given), then carry on from the next line. A script that"
			" waits is linear: each step it carries on from where it waited, keeping its variables, turn and"
			" thrust, rather than starting over.", ""}},
		{"while", {"while condition: run the lines below over and over, as long as it is true. Wait inside it, or"
			" it must finish within the step.", ""}},
		{"loop", {"loop: run the lines below over and over, forever. Wait inside it.", ""}},
		{"routine", {"routine name(parameter, ...): like a function, but run on a line of its own, and it can set"
			" turn and thrust and wait.", ""}},
		{"turn_to", {"turn_to(direction): turn until facing a direction, decided when the turn starts (then turn"
			" stops). Waits.", "while abs(bearing(aim)) > .5: turn = turn_toward(aim); wait"}},
		{"turn_by", {"turn_by(degrees): turn by some degrees (positive is clockwise). Waits.",
			"turn_to(facing rotated by degrees)"}},
		{"fly", {"fly(steps): fly straight for some steps. Waits.", "turn = 0; wait steps"}},
		{"burn", {"burn(steps): full thrust for some steps. Needs \"throttle control\". Waits.",
			"thrust = acceleration; wait steps"}},
		{"coast", {"coast(steps): no thrust for some steps, then full thrust again. Needs \"throttle"
			" control\". Waits.", "thrust = 0; wait steps; thrust = acceleration"}},
		{"track", {"track(steps): home on the target, steering every step, for some steps. Waits.",
			"if target: turn = turn_toward(target.direction); wait (each step)"}},
	};
	return DOCS;
}



const script::Interface &MissileGuidance::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



const MissileGuidance *MissileGuidance::Default(const Weapon &weapon)
{
	// There are only four defaults, so remember where each one is.
	static const MissileGuidance *defaults[2][2] = {};
	const MissileGuidance *&guidance = defaults[weapon.Leading()][weapon.ThrottleControl()];
	if(!guidance)
		guidance = GameData::Guidance().Find(DefaultName(weapon.Leading(), weapon.ThrottleControl()));
	return guidance;
}



bool MissileGuidance::SetPlayerProgram(const Weapon *weapon, const string &text)
{
	auto &programs = PlayerPrograms();
	programs.erase(weapon);
	PlayerProgramTexts().erase(weapon);
	if(!weapon || text.empty())
		return false;
	shared_ptr<MissileGuidance> guidance = Compile(text);
	if(!guidance || !guidance->IsValid() || !guidance->MissingCapability(*weapon).empty())
		return false;
	programs[weapon] = std::move(guidance);
	PlayerProgramTexts()[weapon] = text;
	return true;
}



shared_ptr<const MissileGuidance> MissileGuidance::PlayerProgram(const Weapon *weapon)
{
	const auto &programs = PlayerPrograms();
	auto it = programs.find(weapon);
	return it == programs.end() ? nullptr : it->second;
}



bool MissileGuidance::CanRun(const string &text, const Weapon &weapon)
{
	shared_ptr<MissileGuidance> guidance = Compile(text);
	return guidance && guidance->IsValid() && guidance->MissingCapability(weapon).empty();
}



void MissileGuidance::SetFactionGuidance(bool use)
{
	useFactionGuidance = use;
}



bool MissileGuidance::FactionGuidance()
{
	return useFactionGuidance;
}



string MissileGuidance::PlayerProgramText(const string &name)
{
	for(const auto &[weapon, guidance] : PlayerPrograms())
		if(guidance->Name() == name)
			return PlayerProgramTexts()[weapon];
	return "";
}



void MissileGuidance::ClearPlayerPrograms()
{
	PlayerPrograms().clear();
	PlayerProgramTexts().clear();
}



string MissileGuidance::DefaultName(bool leading, bool throttleControl)
{
	if(leading && throttleControl)
		return "leading and throttle control";
	if(leading)
		return "leading";
	if(throttleControl)
		return "throttle control";
	return "basic";
}



void MissileGuidance::Load(const DataNode &node, const string &name, bool logErrors)
{
	this->name = name;
	program = script::Program();
	program.Load(node, GetInterface(), logErrors);
}



const string &MissileGuidance::Name() const
{
	return name;
}



bool MissileGuidance::IsValid() const
{
	return program.IsValid();
}



const script::Program &MissileGuidance::Program() const
{
	return program;
}



string MissileGuidance::MissingCapability(const Weapon &weapon) const
{
	return MissingCapability(weapon.Leading(), weapon.ThrottleControl(), weapon.OpticalTracking() > 0.);
}



string MissileGuidance::MissingCapability(bool leading, bool throttleControl, bool optical) const
{
	if(!leading && program.Reads(TARGET_VELOCITY))
		return "it reads \"target.velocity\", which requires \"leading\"";
	if(!leading && (program.Reads(TARGET_INTERCEPT) || program.Reads(TARGET_INTERCEPT_STEPS)))
		return "it reads \"target.intercept\", which requires \"leading\"";
	if(!optical && program.Reads(TARGET_FACING))
		return "it reads \"target.facing\", which requires \"optical tracking\"";
	if(!throttleControl && program.Writes(THRUST))
		return "it sets \"thrust\", which requires \"throttle control\"";
	return "";
}



void MissileGuidance::Variables(const Input &input, vector<script::Value> &variables)
{
	if(variables.size() < static_cast<size_t>(THRUST + 1))
		variables.resize(THRUST + 1);
	const bool locked = input.hasLock;
	variables[POSITION] = script::Value(input.position);
	variables[VELOCITY] = script::Value(input.velocity);
	variables[FACING] = script::Value(input.facing);
	variables[HAS_LOCK] = script::Value(locked);
	variables[TARGET_POSITION] = script::Value(locked ? input.targetPosition : Point());
	variables[TARGET_VELOCITY] = script::Value(locked ? input.targetVelocity : Point());
	variables[MAX_TURN] = script::Value(input.maxTurn);
	variables[ACCELERATION] = script::Value(input.acceleration);
	variables[DRAG] = script::Value(input.drag);
	variables[AGE] = script::Value(static_cast<double>(input.age));
	variables[LIFETIME_LEFT] = script::Value(static_cast<double>(input.lifetimeLeft));
	variables[TARGET_FACING] = script::Value(locked ? input.targetFacing : Point());
	// What scripts would otherwise work out for themselves.
	const double topSpeed = (input.drag != 0.) ? input.acceleration / input.drag : input.velocity.Length();
	variables[TOP_SPEED] = script::Value(topSpeed);
	Point offset;
	double distance = 0.;
	Point direction;
	double bearing = 0.;
	double steps = 0.;
	Point intercept;
	double interceptSteps = 0.;
	if(locked)
	{
		offset = input.targetPosition - input.position;
		distance = offset.Length();
		direction = offset.Unit();
		bearing = Bearing(input.facing, direction);
		steps = distance / topSpeed;
		// Where to meet the target. If it is coming toward the projectile, that
		// is where the two have the same velocity normal to the distance
		// between them; otherwise, where the target will be when the
		// projectile gets to where it is now.
		Point meet = offset;
		interceptSteps = steps;
		if(direction.Dot(input.targetVelocity) < 0.)
		{
			const Point normal(direction.Y(), -direction.X());
			const double normalSpeed = normal.Dot(input.targetVelocity);
			const double closingSpeed = sqrt(max(0., topSpeed * topSpeed - normalSpeed * normalSpeed));
			meet = closingSpeed * direction + normalSpeed * normal;
		}
		else
		{
			meet = meet + steps * input.targetVelocity;
			interceptSteps = meet.Length() / topSpeed;
		}
		intercept = meet.Unit();
	}
	variables[TARGET_OFFSET] = script::Value(offset);
	variables[TARGET_DISTANCE] = script::Value(distance);
	variables[TARGET_DIRECTION] = script::Value(direction);
	variables[TARGET_BEARING] = script::Value(bearing);
	variables[TARGET_STEPS_AWAY] = script::Value(steps);
	variables[TARGET_INTERCEPT] = script::Value(intercept);
	variables[TARGET_INTERCEPT_STEPS] = script::Value(interceptSteps);
	variables[TURN] = script::Value(input.maxTurn);
	variables[THRUST] = script::Value(input.acceleration);
}



bool MissileGuidance::Run(const Input &input, Output &output, script::Memory *memory, script::Trace *trace) const
{
	// Each thread reuses its own variables.
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	Variables(input, variables);
	CurrentInput() = &input;
	// (A linear script keeps its turn from step to step, starting with none.)
	variables[TURN] = script::Value(program.IsLinear() ? 0. : input.maxTurn);
	variables[THRUST] = script::Value(input.acceleration);

	uint32_t effects = 0;
	const script::Value &turn = variables[TURN];
	const script::Value &thrust = variables[THRUST];
	output = Output();
	if(!trace)
		trace = script::Activity::Begin("guidance", name);
	if(!program.Run(variables, effects, memory, trace) || turn.GetType() != script::Value::Type::NUMBER
			|| thrust.GetType() != script::Value::Type::NUMBER)
		return false;

	output.dropTarget = effects & (1u << DROP_TARGET);
	// The script can't turn or accelerate faster than the weapon is able to,
	// or thrust backwards. (A turn that is not a number is passed through,
	// as the built-in guidance does.)
	output.turn = turn.Number();
	if(output.turn > input.maxTurn)
		output.turn = input.maxTurn;
	else if(output.turn < -input.maxTurn)
		output.turn = -input.maxTurn;
	output.thrust = thrust.Number();
	if(output.thrust > input.acceleration)
		output.thrust = input.acceleration;
	else if(output.thrust < 0.)
		output.thrust = 0.;
	return true;
}
