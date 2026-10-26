/* ShipManeuver.cpp
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

#include "ShipManeuver.h"

#include "AI.h"
#include "Angle.h"
#include "DataNode.h"

#include <vector>

using namespace std;

namespace {
	using script::Value;

	// The ship being flown by the script that is running on this thread.
	thread_local const ShipManeuver::Context *currentContext = nullptr;

	bool NeedsVector(const Value *arguments, const char *name, string &error)
	{
		if(arguments[0].GetType() == Value::Type::VECTOR && currentContext)
			return true;
		error = currentContext ? string("\"") + name + "\" takes a vector"
			: string("\"") + name + "\" can only be used when flying a ship";
		return false;
	}

	// (stopping-point target-velocity): where the ship would come to rest
	// relative to something moving at the given velocity.
	bool StoppingPoint(const Value *arguments, Value &result, string &error)
	{
		if(!NeedsVector(arguments, "stopping-point", error))
			return false;
		bool shouldReverse = false;
		result = Value(currentContext->StoppingPoint(arguments[0].Vector(), shouldReverse));
		return true;
	}

	// (stopping-reverse target-velocity): whether reverse thrust stops the ship
	// sooner than turning around.
	bool StoppingReverse(const Value *arguments, Value &result, string &error)
	{
		if(!NeedsVector(arguments, "stopping-reverse", error))
			return false;
		bool shouldReverse = false;
		currentContext->StoppingPoint(arguments[0].Vector(), shouldReverse);
		result = Value(shouldReverse);
		return true;
	}

	// (turn-toward v): the turn command that points the ship along v.
	bool TurnToward(const Value *arguments, Value &result, string &error)
	{
		if(!NeedsVector(arguments, "turn-toward", error))
			return false;
		result = Value(currentContext->TurnToward(arguments[0].Vector()));
		return true;
	}

	// (rendezvous-time offset velocity speed): how many steps it takes
	// something moving at the given speed to meet a point at the given offset
	// that moves with the given velocity, or not a number if it never can.
	bool RendezvousTime(const Value *arguments, Value &result, string &error)
	{
		if(arguments[0].GetType() != Value::Type::VECTOR || arguments[1].GetType() != Value::Type::VECTOR
				|| arguments[2].GetType() != Value::Type::NUMBER)
		{
			error = "\"rendezvous-time\" takes two vectors and a number";
			return false;
		}
		result = Value(AI::RendezvousTime(arguments[0].Vector(), arguments[1].Vector(), arguments[2].Number()));
		return true;
	}

	// (heading v): the direction of a vector in degrees, from -180 to 180, with
	// 0 meaning "up", rounded the way the game rounds a ship's facing.
	bool Heading(const Value *arguments, Value &result, string &error)
	{
		if(arguments[0].GetType() != Value::Type::VECTOR)
		{
			error = "\"heading\" takes a vector";
			return false;
		}
		result = Value(Angle(arguments[0].Vector()).Degrees());
		return true;
	}

	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as ShipManeuver::Variable.
		interface.AddInput("position");
		interface.AddInput("velocity");
		interface.AddInput("facing");
		interface.AddInput("facing-degrees");
		interface.AddInput("target-position");
		interface.AddInput("target-velocity");
		interface.AddInput("target-facing");
		interface.AddInput("max-velocity");
		interface.AddInput("acceleration");
		interface.AddInput("turn-rate");
		interface.AddInput("inertial-mass");
		interface.AddInput("drag-force");
		interface.AddInput("reverse-thrust");
		interface.AddInput("last-turn");
		interface.AddInput("should-use-afterburner");
		interface.AddInput("top-speed");
		interface.AddInput("radius");
		interface.AddInput("slow");
		interface.AddInput("cruise-speed");
		interface.AddInput("command-turn");
			interface.AddOutput("turn");
		interface.AddOutput("forward");
		interface.AddOutput("back");
		interface.AddOutput("afterburner");
		interface.AddOutput("arrived");
		interface.AddFunction("rendezvous-time", 3, RendezvousTime);
		interface.AddFunction("heading", 1, Heading);
		interface.AddFunction("stopping-point", 1, StoppingPoint);
		interface.AddFunction("stopping-reverse", 1, StoppingReverse);
		interface.AddFunction("turn-toward", 1, TurnToward);
		return interface;
	}
}



const script::Interface &ShipManeuver::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



void ShipManeuver::Load(const DataNode &node)
{
	program = script::Program();
	program.Load(node, GetInterface());
}



const string &ShipManeuver::Name() const
{
	return program.Name();
}



bool ShipManeuver::IsValid() const
{
	return program.IsValid();
}



bool ShipManeuver::Run(const Input &input, Output &output, const Context *context) const
{
	// Each thread reuses its own variables.
	thread_local vector<Value> variables;
	variables.resize(program.SlotCount());
	variables[POSITION] = Value(input.position);
	variables[VELOCITY] = Value(input.velocity);
	variables[FACING] = Value(input.facing);
	variables[FACING_DEGREES] = Value(input.facingDegrees);
	variables[TARGET_POSITION] = Value(input.targetPosition);
	variables[TARGET_VELOCITY] = Value(input.targetVelocity);
	variables[TARGET_FACING] = Value(input.targetFacing);
	variables[MAX_VELOCITY] = Value(input.maxVelocity);
	variables[ACCELERATION] = Value(input.acceleration);
	variables[TURN_RATE] = Value(input.turnRate);
	variables[INERTIAL_MASS] = Value(input.inertialMass);
	variables[DRAG_FORCE] = Value(input.dragForce);
	variables[REVERSE_THRUST] = Value(input.reverseThrust);
	variables[LAST_TURN] = Value(input.lastTurn);
	variables[SHOULD_USE_AFTERBURNER] = Value(input.shouldUseAfterburner);
	variables[TOP_SPEED] = Value(input.topSpeed);
	variables[RADIUS] = Value(input.radius);
	variables[SLOW] = Value(input.slow);
	variables[CRUISE_SPEED] = Value(input.cruiseSpeed);
	variables[COMMAND_TURN] = Value(input.commandTurn);
	variables[TURN] = Value(input.commandTurn);
	variables[FORWARD] = Value(false);
	variables[BACK] = Value(false);
	variables[AFTERBURNER] = Value(false);
	variables[ARRIVED] = Value(false);

	uint32_t effects = 0;
	output = Output();
	const Context *previousContext = currentContext;
	currentContext = context;
	bool ran = program.Run(variables, effects);
	currentContext = previousContext;
	if(!ran || variables[TURN].GetType() != Value::Type::NUMBER
			|| variables[FORWARD].GetType() != Value::Type::BOOLEAN || variables[BACK].GetType() != Value::Type::BOOLEAN
			|| variables[AFTERBURNER].GetType() != Value::Type::BOOLEAN
			|| variables[ARRIVED].GetType() != Value::Type::BOOLEAN)
		return false;
	output.turn = variables[TURN].Number();
	output.forward = variables[FORWARD].Boolean();
	output.back = variables[BACK].Boolean();
	output.afterburner = variables[AFTERBURNER].Boolean();
	output.arrived = variables[ARRIVED].Boolean();
	return true;
}
