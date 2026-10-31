/* TurretScript.cpp
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

#include "TurretScript.h"

#include "DataNode.h"

#include <vector>

using namespace std;

namespace {
	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as TurretScript::Variable.
		interface.AddInput("previous-aim");
		interface.AddInput("idle-offset");
		interface.AddInput("arc-middle");
		interface.AddOutput("aim");
		return interface;
	}
}



const script::Interface &TurretScript::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



void TurretScript::Load(const DataNode &node)
{
	program = script::Program();
	program.Load(node, GetInterface());
}



const string &TurretScript::Name() const
{
	return program.Name();
}



bool TurretScript::IsValid() const
{
	return program.IsValid();
}



bool TurretScript::Run(const Input &input, double &aim) const
{
	// Each thread reuses its own variables.
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	variables[PREVIOUS_AIM] = script::Value(input.previousAim);
	variables[IDLE_OFFSET] = script::Value(input.idleOffset);
	variables[ARC_MIDDLE] = script::Value(input.arcMiddle);
	variables[AIM] = script::Value(input.previousAim);

	uint32_t effects = 0;
	if(!program.Run(variables, effects, nullptr, script::Activity::Begin("turret", Name()))
			|| variables[AIM].GetType() != script::Value::Type::NUMBER)
		return false;
	aim = variables[AIM].Number();
	return true;
}
