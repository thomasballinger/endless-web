/* EscortRole.h
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

#pragma once

#include "Point.h"
#include "script/Script.h"

#include <memory>
#include <string>

class DataNode;



// A role for one of the player's escorts: a script, loaded from a "role"
// node, that decides each step what the escort does, usually as a list of
// "gambits", checked in order, the first that applies deciding:
//
//   role "Bodyguard"
//   	if hull < .3
//   		retreat
//   	elif threat && threat.distance < 1200
//   		attack_threat
//   	else
//   		station = vec(200, 0)
//   		keep_station
//
// It chooses one of a few behaviors (its effects), which the AI carries out:
// keeping station at a point relative to the leader (in the leader's frame:
// x to its right, y ahead of it), attacking a ship, retreating behind the
// leader, or leaving the decision to the AI's usual rules ("independent").
class EscortRole {
public:
	enum Variable {
		// The escort: where it is and how it moves, how healthy it is (from 0 to
		// 1), and the range of its longest-range weapon.
		POSITION,
		VELOCITY,
		FACING,
		HULL,
		SHIELDS,
		ENERGY,
		WEAPON_RANGE,
		// The leader (the ship the escort follows), and how far away it is.
		LEADER_POSITION,
		LEADER_VELOCITY,
		LEADER_FACING,
		LEADER_HULL,
		LEADER_SHIELDS,
		LEADER_DISTANCE,
		// The leader's target, if it has one: where it is, how far from the
		// leader, and how healthy.
		LEADER_TARGET,
		LEADER_TARGET_POSITION,
		LEADER_TARGET_DISTANCE,
		LEADER_TARGET_HULL,
		// The nearest hostile ship to the leader, if any is within 4000 of it,
		// and whether it is targeting the leader.
		THREAT,
		THREAT_POSITION,
		THREAT_VELOCITY,
		THREAT_DISTANCE,
		THREAT_HULL,
		THREAT_TARGETING_LEADER,
		// The escort's own target, if any.
		TARGET,
		TARGET_POSITION,
		TARGET_DISTANCE,
		TARGET_HULL,
		// This escort's place among the leader's escorts here (from 0), and
		// how many there are.
		ESCORT_INDEX,
		ESCORT_COUNT,
		// Output: where to keep station, relative to the leader.
		STATION
	};
	enum Effect {
		KEEP_STATION,
		ATTACK_THREAT,
		ATTACK_LEADER_TARGET,
		ATTACK_TARGET,
		RETREAT,
		INDEPENDENT
	};

	class Input {
	public:
		Point position;
		Point velocity;
		Point facing;
		double hull = 0.;
		double shields = 0.;
		double energy = 0.;
		double weaponRange = 0.;
		Point leaderPosition;
		Point leaderVelocity;
		Point leaderFacing;
		double leaderHull = 0.;
		double leaderShields = 0.;
		bool leaderTarget = false;
		Point leaderTargetPosition;
		double leaderTargetHull = 0.;
		bool threat = false;
		Point threatPosition;
		Point threatVelocity;
		double threatHull = 0.;
		bool threatTargetingLeader = false;
		bool target = false;
		Point targetPosition;
		double targetHull = 0.;
		int escortIndex = 0;
		int escortCount = 1;
	};
	// What the role decided: one behavior (the first, in the order of Effect,
	// if it chose more than one), and where to keep station.
	class Output {
	public:
		Effect behavior = INDEPENDENT;
		bool chose = false;
		Point station = Point(0., -250.);
	};

	static const script::Interface &GetInterface();


public:
	void Load(const DataNode &node, bool logErrors = true);

	const std::string &Name() const;
	bool IsValid() const;
	const script::Program &Program() const;

	// Run the role with the given memory (which one escort keeps from step to
	// step). Returns false if the script fails.
	bool Run(const Input &input, Output &output, script::Memory *memory = nullptr,
		script::Trace *trace = nullptr) const;

	// The role the player gave one of their escorts (by its UUID, or "*" for
	// every escort without a role of its own), as the text of a "role" node,
	// or none (with empty text). Each escort keeps its own memory for its
	// role. Returns whether the text compiled; one that doesn't removes the
	// role.
	static bool SetPlayerRole(const std::string &uuid, const std::string &text);
	static const EscortRole *PlayerRole(const std::string &uuid, script::Memory **memory = nullptr);
	// The text of a role the player gave an escort, by the role's name, or
	// empty if no escort has it.
	static std::string PlayerRoleText(const std::string &name);
	static void ClearPlayerRoles();


private:
	script::Program program;
};
