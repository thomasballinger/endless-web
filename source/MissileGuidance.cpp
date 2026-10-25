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

#include "DataNode.h"

using namespace std;

namespace {
	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as MissileGuidance::Variable.
		interface.AddInput("position");
		interface.AddInput("velocity");
		interface.AddInput("facing");
		interface.AddInput("has-target");
		interface.AddInput("target-position");
		interface.AddInput("target-velocity");
		interface.AddInput("has-lock");
		interface.AddInput("confused");
		interface.AddInput("confusion-direction");
		interface.AddInput("max-turn");
		interface.AddInput("acceleration");
		interface.AddInput("drag");
		interface.AddInput("blindspot");
		interface.AddInput("leading");
		interface.AddInput("throttle-control");
		interface.AddOutput("turn");
		interface.AddOutput("thrust");
		interface.AddEffect("drop-target");
		return interface;
	}
}



const script::Interface &MissileGuidance::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



void MissileGuidance::Load(const DataNode &node)
{
	program = script::Program();
	program.Load(node, GetInterface());
}



const string &MissileGuidance::Name() const
{
	return program.Name();
}



bool MissileGuidance::IsValid() const
{
	return program.IsValid();
}



const script::Program &MissileGuidance::Program() const
{
	return program;
}
