/* AIRules.h
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

#include <cstdint>
#include <string>
#include <vector>

class DataNode;



// An ordered list of rules that decide what an AI-controlled ship does each
// step, loaded from an "ai" node in the game data. Each rule names a built-in
// action and lists conditions that must all hold for the action to run.
//
// A rule set has two lists. The "turn" rules run in order every step; some
// actions end the ship's turn early. One of those actions, "move", runs the
// "movement" rules, which choose how the ship navigates: the first movement
// rule whose conditions hold and whose action succeeds wins.
class AIRules {
public:
	// Actions available to "turn" rules.
	enum class TurnAction {
		ASK_FOR_HELP,
		ACT_DISABLED,
		END_TURN,
		PREPARE_COMMANDS,
		CLOAK,
		UPDATE_FLEET_AND_TARGETS,
		COLLECT_FLOTSAM,
		TRACK_SCANS,
		CHOOSE_TARGET,
		AIM_AND_FIRE,
		WAIT_IN_TRANSIT,
		CONSIDER_FLEEING,
		REACT_TO_DAMAGE,
		ASSIST,
		ASSESS_SITUATION,
		SWARM,
		ACT_SECRETIVE,
		SURVEIL,
		HARVEST,
		MINE,
		ACT_CARRIED,
		CHECK_RECALL,
		MOVE,
		SCATTER
	};

	// Actions available to "movement" rules.
	enum class MovementAction {
		STOP_AND_WAIT,
		FOLLOW_ORDERS,
		INDEPENDENT,
		ESCORT,
		ASSIST_PARENT,
		CIRCLE_PARENT
	};

	// Facts about the ship and its situation that conditions can test.
	enum class Predicate {
		PRESENT,
		YOURS,
		SPECIAL,
		DISABLED,
		STRANDED,
		NEEDS_HELP,
		OVERHEATED,
		PLAYER_CLOAKING,
		IN_TRANSIT,
		HAS_TARGET,
		HAS_FLOTSAM_TARGET,
		TARGET_NEAR,
		SHOULD_FLEE,
		HEAVILY_DAMAGED,
		INJURED,
		STRANDED_WITH_HELPER,
		SCAN_PERMISSION,
		MINING_ALLOWED,
		CAN_BE_CARRIED,
		HAS_FUEL_CAPACITY,
		MUST_RECALL,
		HAS_PARENT,
		PARENT_ELSEWHERE,
		PARENT_HOSTILE,
		PARENT_TARGETABLE,
		CAN_PURSUE_PARENT,
		PARENT_DISABLED,
		PARENT_DERELICT,
		PARENT_JUMPING,
		PARENT_FAR,
		PERSONALITY
	};

	class Condition {
	public:
		Predicate predicate;
		bool negated = false;
		// For PERSONALITY conditions, the index of the trait to test.
		int trait = -1;
	};

	template<class Action>
	class Rule {
	public:
		Action action;
		std::vector<Condition> conditions;
		// Text describing this rule, for diagnostics.
		std::string description;
	};


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool HasTurnRules() const;
	bool HasMovementRules() const;
	const std::vector<Rule<TurnAction>> &TurnRules() const;
	const std::vector<Rule<MovementAction>> &MovementRules() const;


private:
	std::string name;
	bool hasTurnRules = false;
	bool hasMovementRules = false;
	std::vector<Rule<TurnAction>> turnRules;
	std::vector<Rule<MovementAction>> movementRules;
};
