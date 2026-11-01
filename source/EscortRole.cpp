/* EscortRole.cpp
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

#include "EscortRole.h"

#include "DataFile.h"
#include "DataNode.h"

#include <map>
#include <sstream>
#include <vector>

using namespace std;

namespace {
	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as EscortRole::Variable.
		for(const char *name : {"position", "velocity", "facing", "hull", "shields", "energy", "weapon-range",
				"leader-position", "leader-velocity", "leader-facing", "leader-hull", "leader-shields",
				"leader-distance", "leader-target", "leader-target-position", "leader-target-distance",
				"leader-target-hull", "threat", "threat-position", "threat-velocity", "threat-distance",
				"threat-hull", "threat-targeting-leader", "target", "target-position", "target-distance",
				"target-hull", "escort-index", "escort-count"})
			interface.AddInput(name);
		interface.AddOutput("station");
		// These must be added in the same order as EscortRole::Effect.
		for(const char *name : {"keep-station", "attack-threat", "attack-leader-target", "attack-target", "retreat",
				"independent"})
			interface.AddEffect(name);
		// What is known of a ship is only known while there is one.
		for(const char *name : {"leader-target-position", "leader-target-distance", "leader-target-hull"})
			interface.SetGuard(interface.Find(name), interface.Find("leader-target"));
		for(const char *name : {"threat-position", "threat-velocity", "threat-distance", "threat-hull",
				"threat-targeting-leader"})
			interface.SetGuard(interface.Find(name), interface.Find("threat"));
		for(const char *name : {"target-position", "target-distance", "target-hull"})
			interface.SetGuard(interface.Find(name), interface.Find("target"));
		return interface;
	}

	// The roles the player has given their escorts, by escort ("*" for all
	// those without one of their own), and the memory each escort keeps for
	// its role. (A memory resets itself when the program it was kept for
	// changes.)
	class Assigned {
	public:
		shared_ptr<EscortRole> role;
		string text;
	};
	map<string, Assigned> &PlayerRoles()
	{
		static map<string, Assigned> roles;
		return roles;
	}
	map<string, script::Memory> &Memories()
	{
		static map<string, script::Memory> memories;
		return memories;
	}
}



const script::Interface &EscortRole::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



void EscortRole::Load(const DataNode &node, bool logErrors)
{
	program = script::Program();
	program.Load(node, GetInterface(), logErrors);
}



const string &EscortRole::Name() const
{
	return program.Name();
}



bool EscortRole::IsValid() const
{
	return program.IsValid();
}



const script::Program &EscortRole::Program() const
{
	return program;
}



bool EscortRole::Run(const Input &input, Output &output, script::Memory *memory, script::Trace *trace) const
{
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	auto set = [](int slot, const auto &value) { variables[slot] = script::Value(value); };
	set(POSITION, input.position);
	set(VELOCITY, input.velocity);
	set(FACING, input.facing);
	set(HULL, input.hull);
	set(SHIELDS, input.shields);
	set(ENERGY, input.energy);
	set(WEAPON_RANGE, input.weaponRange);
	set(LEADER_POSITION, input.leaderPosition);
	set(LEADER_VELOCITY, input.leaderVelocity);
	set(LEADER_FACING, input.leaderFacing);
	set(LEADER_HULL, input.leaderHull);
	set(LEADER_SHIELDS, input.leaderShields);
	set(LEADER_DISTANCE, input.position.Distance(input.leaderPosition));
	set(LEADER_TARGET, input.leaderTarget);
	set(LEADER_TARGET_POSITION, input.leaderTargetPosition);
	set(LEADER_TARGET_DISTANCE, input.leaderTargetPosition.Distance(input.leaderPosition));
	set(LEADER_TARGET_HULL, input.leaderTargetHull);
	set(THREAT, input.threat);
	set(THREAT_POSITION, input.threatPosition);
	set(THREAT_VELOCITY, input.threatVelocity);
	set(THREAT_DISTANCE, input.threatPosition.Distance(input.leaderPosition));
	set(THREAT_HULL, input.threatHull);
	set(THREAT_TARGETING_LEADER, input.threatTargetingLeader);
	set(TARGET, input.target);
	set(TARGET_POSITION, input.targetPosition);
	set(TARGET_DISTANCE, input.targetPosition.Distance(input.position));
	set(TARGET_HULL, input.targetHull);
	set(ESCORT_INDEX, static_cast<double>(input.escortIndex));
	set(ESCORT_COUNT, static_cast<double>(input.escortCount));
	set(STATION, Point(0., -250.));

	output = Output();
	uint32_t effects = 0;
	if(!trace)
		trace = script::Activity::Begin("role", Name());
	if(!program.Run(variables, effects, memory, trace))
		return false;
	if(variables[STATION].GetType() == script::Value::Type::VECTOR)
		output.station = variables[STATION].Vector();
	for(int effect = KEEP_STATION; effect <= INDEPENDENT; ++effect)
		if(effects & (1u << effect))
		{
			output.behavior = static_cast<Effect>(effect);
			output.chose = true;
			break;
		}
	return true;
}



bool EscortRole::SetPlayerRole(const string &uuid, const string &text)
{
	map<string, Assigned> &roles = PlayerRoles();
	roles.erase(uuid);
	if(text.empty())
		return false;
	istringstream in(text);
	DataFile file(in);
	for(const DataNode &node : file)
		if(node.Token(0) == "role" && node.Size() >= 2)
		{
			auto role = make_shared<EscortRole>();
			role->Load(node, false);
			if(!role->IsValid())
				return false;
			roles[uuid] = {std::move(role), text};
			return true;
		}
	return false;
}



const EscortRole *EscortRole::PlayerRole(const string &uuid, script::Memory **memory)
{
	map<string, Assigned> &roles = PlayerRoles();
	auto it = roles.find(uuid);
	if(it == roles.end())
		it = roles.find("*");
	if(it == roles.end())
		return nullptr;
	if(memory)
		*memory = &Memories()[uuid];
	return it->second.role.get();
}



string EscortRole::PlayerRoleText(const string &name)
{
	for(const auto &[uuid, assigned] : PlayerRoles())
		if(assigned.role->Name() == name)
			return assigned.text;
	return "";
}



void EscortRole::ClearPlayerRoles()
{
	PlayerRoles().clear();
	Memories().clear();
}
