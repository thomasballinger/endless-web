/* test_aiRules.cpp
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

#include "es-test.hpp"

// Include only the tested class's header.
#include "../../../source/AIRules.h"

// Include a helper for creating well-formed DataNodes.
#include "datanode-factory.h"

#include "../../../source/Personality.h"

#include <string>

namespace { // test namespace

// #region mock data
std::string rules_with_both_lists =
R"(ai "escorts"
	turn
		"prepare commands"
		"choose target"
			present
			not personality swarming
		move
	movement
		"follow orders"
		escort
			"parent elsewhere"
		independent
)";

std::string rules_with_mistakes =
R"(ai "mistakes"
	turn
		"prepare commands"
		"fly in circles"
		"choose target"
			present
			"feeling lucky"
		"aim and fire"
			personality grumpy
		move
)";

std::string movement_only =
R"(ai "player escorts"
	movement
		escort
)";
// #endregion mock data



// #region unit tests
SCENARIO( "Loading AI rules", "[AIRules]" ) {
	GIVEN( "a rule set with turn and movement lists" ) {
		AIRules rules;
		rules.Load(AsDataNode(rules_with_both_lists));
		THEN( "both lists are loaded in order" ) {
			REQUIRE( rules.Name() == "escorts" );
			REQUIRE( rules.HasTurnRules() );
			REQUIRE( rules.HasMovementRules() );
			REQUIRE( rules.TurnRules().size() == 3 );
			REQUIRE( rules.TurnRules()[0].action == AIRules::TurnAction::PREPARE_COMMANDS );
			REQUIRE( rules.TurnRules()[1].action == AIRules::TurnAction::CHOOSE_TARGET );
			REQUIRE( rules.TurnRules()[2].action == AIRules::TurnAction::MOVE );
			REQUIRE( rules.MovementRules().size() == 3 );
			REQUIRE( rules.MovementRules()[0].action == AIRules::MovementAction::FOLLOW_ORDERS );
			REQUIRE( rules.MovementRules()[2].action == AIRules::MovementAction::INDEPENDENT );
		}
		THEN( "conditions are parsed, including negation and personality traits" ) {
			const auto &conditions = rules.TurnRules()[1].conditions;
			REQUIRE( conditions.size() == 2 );
			CHECK( conditions[0].predicate == AIRules::Predicate::PRESENT );
			CHECK_FALSE( conditions[0].negated );
			CHECK( conditions[1].predicate == AIRules::Predicate::PERSONALITY );
			CHECK( conditions[1].negated );
			CHECK( conditions[1].trait == Personality::TraitIndex("swarming") );
			CHECK( rules.TurnRules()[1].description == "choose target if present, not personality swarming" );
		}
	}
	GIVEN( "a rule set with unknown actions and conditions" ) {
		AIRules rules;
		rules.Load(AsDataNode(rules_with_mistakes));
		THEN( "rules that can't be understood are skipped, rather than run unconditionally" ) {
			REQUIRE( rules.TurnRules().size() == 2 );
			CHECK( rules.TurnRules()[0].action == AIRules::TurnAction::PREPARE_COMMANDS );
			CHECK( rules.TurnRules()[1].action == AIRules::TurnAction::MOVE );
		}
	}
	GIVEN( "a rule set that only defines movement" ) {
		AIRules rules;
		rules.Load(AsDataNode(movement_only));
		THEN( "it reports that it has no turn rules, so the default ones are used" ) {
			CHECK_FALSE( rules.HasTurnRules() );
			REQUIRE( rules.HasMovementRules() );
			CHECK( rules.MovementRules().size() == 1 );
		}
	}
}
// #endregion unit tests



} // test namespace
