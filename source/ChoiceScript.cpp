/* ChoiceScript.cpp
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

#include "ChoiceScript.h"

#include "AI.h"
#include "DataNode.h"

#include <cmath>
#include <vector>

using namespace std;

namespace {
	// (rendezvous-time offset velocity speed), as for maneuvers.
	bool RendezvousTime(const script::Value *arguments, script::Value &result, string &error)
	{
		if(arguments[0].GetType() != script::Value::Type::VECTOR || arguments[1].GetType() != script::Value::Type::VECTOR
				|| arguments[2].GetType() != script::Value::Type::NUMBER)
		{
			error = "\"rendezvous-time\" takes two vectors and a number";
			return false;
		}
		result = script::Value(AI::RendezvousTime(arguments[0].Vector(), arguments[1].Vector(), arguments[2].Number()));
		return true;
	}

	script::Interface MakeInterface(ChoiceScript::Kind kind)
	{
		script::Interface interface;
		// These must be added in the same order as ChoiceScript::Variable.
		interface.AddInput("position");
		interface.AddInput("velocity");
		interface.AddInput("facing");
		interface.AddInput("max-velocity");
		interface.AddInput("turn-rate");
		interface.AddInput("yours");
		if(kind == ChoiceScript::Kind::RATE)
		{
			interface.AddInput("candidate-position");
			interface.AddInput("candidate-velocity");
			interface.AddOutput("score");
			interface.AddEffect("skip");
		}
		else
		{
			interface.AddList("flotsam", {"position", "velocity"});
			interface.AddOutput("chosen");
		}
		interface.AddFunction("rendezvous-time", 3, RendezvousTime);
		return interface;
	}

	void SetShipInputs(const ChoiceScript::Input &input, vector<script::Value> &variables)
	{
		variables[ChoiceScript::POSITION] = script::Value(input.position);
		variables[ChoiceScript::VELOCITY] = script::Value(input.velocity);
		variables[ChoiceScript::FACING] = script::Value(input.facing);
		variables[ChoiceScript::MAX_VELOCITY] = script::Value(input.maxVelocity);
		variables[ChoiceScript::TURN_RATE] = script::Value(input.turnRate);
		variables[ChoiceScript::YOURS] = script::Value(input.yours);
	}
}



const script::Interface &ChoiceScript::GetInterface(Kind kind)
{
	static const script::Interface rate = MakeInterface(Kind::RATE);
	static const script::Interface choose = MakeInterface(Kind::CHOOSE);
	return kind == Kind::RATE ? rate : choose;
}



void ChoiceScript::Load(const DataNode &node, Kind kind)
{
	this->kind = kind;
	program = script::Program();
	program.Load(node, GetInterface(kind));
}



const string &ChoiceScript::Name() const
{
	return program.Name();
}



bool ChoiceScript::IsValid() const
{
	return program.IsValid();
}



bool ChoiceScript::Rate(const Input &input, double &score) const
{
	if(kind != Kind::RATE)
		return false;
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	SetShipInputs(input, variables);
	variables[CANDIDATE_POSITION] = script::Value(input.candidatePosition);
	variables[CANDIDATE_VELOCITY] = script::Value(input.candidateVelocity);
	variables[SCORE] = script::Value(0.);

	uint32_t effects = 0;
	if(!program.Run(variables, effects, nullptr, script::Activity::Begin("rate", Name())) || (effects & (1u << SKIP))
			|| variables[SCORE].GetType() != script::Value::Type::NUMBER)
		return false;
	score = variables[SCORE].Number();
	return true;
}



int ChoiceScript::Choose(const Input &input) const
{
	if(kind != Kind::CHOOSE || !input.candidates)
		return -1;
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	SetShipInputs(input, variables);
	variables[CANDIDATES] = script::Value(*input.candidates);
	variables[CHOSEN] = script::Value(-1.);

	uint32_t effects = 0;
	if(!program.Run(variables, effects, nullptr, script::Activity::Begin("choose", Name()))
			|| variables[CHOSEN].GetType() != script::Value::Type::NUMBER)
		return -1;
	const double chosen = variables[CHOSEN].Number();
	if(!(chosen >= 0.) || chosen >= static_cast<double>(input.candidates->Size()) || chosen != floor(chosen))
		return -1;
	return static_cast<int>(chosen);
}
