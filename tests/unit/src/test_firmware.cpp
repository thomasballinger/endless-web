/* test_firmware.cpp
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
#include "../../../source/Firmware.h"

// Include a helper for creating well-formed DataNodes.
#include "datanode-factory.h"

#include <string>

namespace { // test namespace

// #region unit tests
SCENARIO( "Reading a program's data", "[Firmware]" ) {
	GIVEN( "a guidance program" ) {
		Firmware firmware;
		firmware.Load(AsDataNode("program Corkscrew\n\tguidance\n\tauthor Pirate\n\t\"flown by\" Pirate Marauder\n"
			"\tprice 18000\n\tdescription \"Weaves.\"\n"));
		THEN( "it knows what it is, and who wrote it" ) {
			CHECK( firmware.Name() == "Corkscrew" );
			CHECK( firmware.IsGuidance() );
			CHECK_FALSE( firmware.IsDaemon() );
			CHECK( firmware.Author() == "Pirate" );
			CHECK( firmware.Price() == 18000 );
			CHECK( firmware.Description() == "Weaves." );
		}
	}
	GIVEN( "a maneuver and a daemon" ) {
		Firmware maneuver;
		maneuver.Load(AsDataNode("program Wingman\n\tmaneuver \"keep station\"\n"));
		Firmware daemon;
		daemon.Load(AsDataNode("program Triage\n\tdaemon\n"));
		THEN( "each knows its kind" ) {
			CHECK( maneuver.Kind() == "maneuver" );
			CHECK( maneuver.Replaces() == "keep station" );
			CHECK( daemon.Kind() == "daemon" );
			CHECK( daemon.IsDaemon() );
		}
	}
}

SCENARIO( "Reading and renaming a program's text", "[Firmware]" ) {
	GIVEN( "the text of a program, with comments before it" ) {
		const std::string text = "# Made in a shipyard.\n# Do not modify.\nmaneuver \"fight\"\n\tif ramming\n\t\tattack\n";
		THEN( "its kind and name come from its node" ) {
			CHECK( Firmware::KindOf(text) == "maneuver" );
			CHECK( Firmware::NameOf(text) == "fight" );
		}
		THEN( "renaming it renames the node, keeping the comments and the code" ) {
			const std::string renamed = Firmware::Renamed(text, "fight (copied)");
			const std::string expected = "# Made in a shipyard.\n# Do not modify.\nmaneuver \"fight (copied)\"\n"
				"\tif ramming\n\t\tattack\n";
			CHECK( renamed == expected );
			CHECK( Firmware::NameOf(renamed) == "fight (copied)" );
		}
	}
	GIVEN( "text that isn't a program" ) {
		const std::string text = "ship Sparrow\n";
		THEN( "it has no kind, and renaming leaves it as it is" ) {
			CHECK( Firmware::KindOf(text).empty() );
			CHECK( Firmware::Renamed(text, "x") == text );
		}
	}
}
// #endregion unit tests



} // test namespace
