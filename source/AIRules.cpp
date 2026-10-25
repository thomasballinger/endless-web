/* AIRules.cpp
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

#include "AIRules.h"

#include "DataNode.h"
#include "Personality.h"

#include <map>

using namespace std;

namespace {
	const map<string, AIRules::TurnAction> TURN_ACTIONS = {
		{"ask for help", AIRules::TurnAction::ASK_FOR_HELP},
		{"act disabled", AIRules::TurnAction::ACT_DISABLED},
		{"end turn", AIRules::TurnAction::END_TURN},
		{"prepare commands", AIRules::TurnAction::PREPARE_COMMANDS},
		{"cloak", AIRules::TurnAction::CLOAK},
		{"update fleet and targets", AIRules::TurnAction::UPDATE_FLEET_AND_TARGETS},
		{"collect flotsam", AIRules::TurnAction::COLLECT_FLOTSAM},
		{"track scans", AIRules::TurnAction::TRACK_SCANS},
		{"choose target", AIRules::TurnAction::CHOOSE_TARGET},
		{"aim and fire", AIRules::TurnAction::AIM_AND_FIRE},
		{"wait in transit", AIRules::TurnAction::WAIT_IN_TRANSIT},
		{"consider fleeing", AIRules::TurnAction::CONSIDER_FLEEING},
		{"react to damage", AIRules::TurnAction::REACT_TO_DAMAGE},
		{"assist", AIRules::TurnAction::ASSIST},
		{"assess situation", AIRules::TurnAction::ASSESS_SITUATION},
		{"swarm", AIRules::TurnAction::SWARM},
		{"act secretive", AIRules::TurnAction::ACT_SECRETIVE},
		{"surveil", AIRules::TurnAction::SURVEIL},
		{"harvest", AIRules::TurnAction::HARVEST},
		{"mine", AIRules::TurnAction::MINE},
		{"act carried", AIRules::TurnAction::ACT_CARRIED},
		{"check recall", AIRules::TurnAction::CHECK_RECALL},
		{"move", AIRules::TurnAction::MOVE},
		{"scatter", AIRules::TurnAction::SCATTER},
	};

	const map<string, AIRules::MovementAction> MOVEMENT_ACTIONS = {
		{"stop and wait", AIRules::MovementAction::STOP_AND_WAIT},
		{"follow orders", AIRules::MovementAction::FOLLOW_ORDERS},
		{"independent", AIRules::MovementAction::INDEPENDENT},
		{"escort", AIRules::MovementAction::ESCORT},
		{"assist parent", AIRules::MovementAction::ASSIST_PARENT},
		{"circle parent", AIRules::MovementAction::CIRCLE_PARENT},
		{"route to parent", AIRules::MovementAction::ROUTE_TO_PARENT},
		{"land at target", AIRules::MovementAction::LAND_AT_TARGET},
		{"jump", AIRules::MovementAction::JUMP},
		{"refuel", AIRules::MovementAction::REFUEL},
		{"park", AIRules::MovementAction::PARK},
		{"route to parent's destination", AIRules::MovementAction::ROUTE_TO_PARENT_DESTINATION},
		{"jump with parent", AIRules::MovementAction::JUMP_WITH_PARENT},
		{"approach landing target", AIRules::MovementAction::APPROACH_LANDING_TARGET},
		{"keep station", AIRules::MovementAction::KEEP_STATION},
		{"land with parent", AIRules::MovementAction::LAND_WITH_PARENT},
		{"hold for boarding", AIRules::MovementAction::HOLD_FOR_BOARDING},
		{"move with parent", AIRules::MovementAction::MOVE_WITH_PARENT},
	};

	const map<string, AIRules::Predicate> PREDICATES = {
		{"present", AIRules::Predicate::PRESENT},
		{"yours", AIRules::Predicate::YOURS},
		{"special", AIRules::Predicate::SPECIAL},
		{"disabled", AIRules::Predicate::DISABLED},
		{"stranded", AIRules::Predicate::STRANDED},
		{"needs help", AIRules::Predicate::NEEDS_HELP},
		{"overheated", AIRules::Predicate::OVERHEATED},
		{"player cloaking", AIRules::Predicate::PLAYER_CLOAKING},
		{"in transit", AIRules::Predicate::IN_TRANSIT},
		{"has target", AIRules::Predicate::HAS_TARGET},
		{"has flotsam target", AIRules::Predicate::HAS_FLOTSAM_TARGET},
		{"target near", AIRules::Predicate::TARGET_NEAR},
		{"should flee", AIRules::Predicate::SHOULD_FLEE},
		{"heavily damaged", AIRules::Predicate::HEAVILY_DAMAGED},
		{"injured", AIRules::Predicate::INJURED},
		{"stranded with helper", AIRules::Predicate::STRANDED_WITH_HELPER},
		{"scan permission", AIRules::Predicate::SCAN_PERMISSION},
		{"mining allowed", AIRules::Predicate::MINING_ALLOWED},
		{"can be carried", AIRules::Predicate::CAN_BE_CARRIED},
		{"has fuel capacity", AIRules::Predicate::HAS_FUEL_CAPACITY},
		{"must recall", AIRules::Predicate::MUST_RECALL},
		{"has parent", AIRules::Predicate::HAS_PARENT},
		{"parent elsewhere", AIRules::Predicate::PARENT_ELSEWHERE},
		{"parent hostile", AIRules::Predicate::PARENT_HOSTILE},
		{"parent targetable", AIRules::Predicate::PARENT_TARGETABLE},
		{"can pursue parent", AIRules::Predicate::CAN_PURSUE_PARENT},
		{"parent disabled", AIRules::Predicate::PARENT_DISABLED},
		{"parent derelict", AIRules::Predicate::PARENT_DERELICT},
		{"parent jumping", AIRules::Predicate::PARENT_JUMPING},
		{"parent far", AIRules::Predicate::PARENT_FAR},
		{"parent hidden", AIRules::Predicate::PARENT_HIDDEN},
		{"parent has destination", AIRules::Predicate::PARENT_HAS_DESTINATION},
		{"parent landing", AIRules::Predicate::PARENT_LANDING},
		{"parent planet here", AIRules::Predicate::PARENT_PLANET_HERE},
		{"parent boarding me", AIRules::Predicate::PARENT_BOARDING_ME},
		{"needs fuel", AIRules::Predicate::NEEDS_FUEL},
		{"can refuel here", AIRules::Predicate::CAN_REFUEL_HERE},
		{"fuel not full", AIRules::Predicate::FUEL_NOT_FULL},
		{"has landing target", AIRules::Predicate::HAS_LANDING_TARGET},
		{"has jump target", AIRules::Predicate::HAS_JUMP_TARGET},
		{"jumps remaining", AIRules::Predicate::JUMPS_REMAINING},
	};



	// Parse one condition line, e.g. "present", "not yours", "personality timid".
	bool ParseCondition(const DataNode &node, AIRules::Condition &condition)
	{
		int index = 0;
		if(node.Size() > 1 && node.Token(0) == "not")
		{
			condition.negated = true;
			++index;
		}
		const string &key = node.Token(index);
		if(key == "personality")
		{
			if(node.Size() != index + 2)
			{
				node.PrintTrace("Expected a single personality trait:");
				return false;
			}
			condition.predicate = AIRules::Predicate::PERSONALITY;
			condition.trait = Personality::TraitIndex(node.Token(index + 1));
			if(condition.trait < 0)
			{
				node.PrintTrace("Unknown personality trait:");
				return false;
			}
			return true;
		}
		auto it = PREDICATES.find(key);
		if(it == PREDICATES.end() || node.Size() != index + 1)
		{
			node.PrintTrace("Unrecognized AI rule condition:");
			return false;
		}
		condition.predicate = it->second;
		return true;
	}



	template<class Action>
	void LoadRules(const DataNode &node, const map<string, Action> &actions,
		vector<AIRules::Rule<Action>> &rules)
	{
		rules.clear();
		for(const DataNode &child : node)
		{
			auto it = actions.find(child.Token(0));
			if(it == actions.end() || child.Size() != 1)
			{
				child.PrintTrace("Unrecognized AI rule action:");
				continue;
			}
			AIRules::Rule<Action> rule;
			rule.action = it->second;
			rule.description = child.Token(0);
			bool valid = true;
			for(const DataNode &grand : child)
			{
				AIRules::Condition condition;
				if(ParseCondition(grand, condition))
				{
					rule.conditions.push_back(condition);
					rule.description += (rule.conditions.size() == 1 ? " if " : ", ");
					for(int i = 0; i < grand.Size(); ++i)
						rule.description += (i ? " " : "") + grand.Token(i);
				}
				else
					valid = false;
			}
			// A rule with a condition that couldn't be understood would run
			// more often than intended, so skip it entirely.
			if(valid)
				rules.push_back(std::move(rule));
		}
	}
}



void AIRules::Load(const DataNode &node)
{
	if(node.Size() >= 2)
		name = node.Token(1);

	// Each list replaces any previous definition, so a plugin can redefine
	// one list of a rule set without repeating the other.
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		if(key == "turn")
		{
			LoadRules(child, TURN_ACTIONS, turnRules);
			hasTurnRules = true;
		}
		else if(key == "movement")
		{
			LoadRules(child, MOVEMENT_ACTIONS, movementRules);
			hasMovementRules = true;
		}
		else if(key == "escort")
		{
			LoadRules(child, MOVEMENT_ACTIONS, escortRules);
			hasEscortRules = true;
		}
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



const string &AIRules::Name() const
{
	return name;
}



bool AIRules::HasTurnRules() const
{
	return hasTurnRules;
}



bool AIRules::HasMovementRules() const
{
	return hasMovementRules;
}



bool AIRules::HasEscortRules() const
{
	return hasEscortRules;
}



const vector<AIRules::Rule<AIRules::TurnAction>> &AIRules::TurnRules() const
{
	return turnRules;
}



const vector<AIRules::Rule<AIRules::MovementAction>> &AIRules::MovementRules() const
{
	return movementRules;
}



const vector<AIRules::Rule<AIRules::MovementAction>> &AIRules::EscortRules() const
{
	return escortRules;
}
