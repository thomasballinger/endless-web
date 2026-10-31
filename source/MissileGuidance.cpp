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
#include "Weapon.h"

#include <map>
#include <sstream>
#include <vector>

using namespace std;

namespace {
	// The guidance programs the player has written, by weapon, and their text.
	map<const Weapon *, MissileGuidance> &PlayerPrograms()
	{
		static map<const Weapon *, MissileGuidance> programs;
		return programs;
	}

	map<const Weapon *, string> &PlayerProgramTexts()
	{
		static map<const Weapon *, string> texts;
		return texts;
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
		interface.AddOutput("turn");
		interface.AddOutput("thrust");
		interface.AddEffect("drop-target");
		// In infix scripts, what the seeker sees of its target is
		// "target.position" and so on, which can only be read inside
		// "if target" (that is, while it has a lock).
		const int hasLock = interface.Find("has-lock");
		interface.AddAlias("target", hasLock);
		for(const char *input : {"target-position", "target-velocity", "target-facing"})
			interface.SetGuard(interface.Find(input), hasLock);
		return interface;
	}
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
	map<const Weapon *, MissileGuidance> &programs = PlayerPrograms();
	programs.erase(weapon);
	PlayerProgramTexts().erase(weapon);
	if(!weapon || text.empty())
		return false;
	istringstream in(text);
	DataFile file(in);
	for(const DataNode &node : file)
		if(node.Token(0) == "guidance" && node.Size() >= 2)
		{
			MissileGuidance guidance;
			guidance.Load(node, node.Token(1), false);
			if(!guidance.IsValid() || !guidance.MissingCapability(*weapon).empty())
				return false;
			programs[weapon] = std::move(guidance);
			PlayerProgramTexts()[weapon] = text;
			return true;
		}
	return false;
}



const MissileGuidance *MissileGuidance::PlayerProgram(const Weapon *weapon)
{
	const map<const Weapon *, MissileGuidance> &programs = PlayerPrograms();
	auto it = programs.find(weapon);
	return it == programs.end() ? nullptr : &it->second;
}



string MissileGuidance::PlayerProgramText(const string &name)
{
	for(const auto &[weapon, guidance] : PlayerPrograms())
		if(guidance.Name() == name)
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
		return "it reads \"target-velocity\", which requires \"leading\"";
	if(!optical && program.Reads(TARGET_FACING))
		return "it reads \"target-facing\", which requires \"optical tracking\"";
	if(!throttleControl && program.Writes(THRUST))
		return "it sets \"thrust\", which requires \"throttle control\"";
	return "";
}



bool MissileGuidance::Run(const Input &input, Output &output, script::Memory *memory, script::Trace *trace) const
{
	// Each thread reuses its own variables.
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
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
	variables[TURN] = script::Value(input.maxTurn);
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
