/* test_codeStyle.cpp
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
#include "../../../source/CodeStyle.h"

// Include a helper for creating well-formed DataNodes.
#include "datanode-factory.h"

#include <string>

namespace { // test namespace

// #region mock data
CodeStyle MakeStyle(const std::string &text)
{
	CodeStyle style;
	style.Load(AsDataNode(text));
	return style;
}
// #endregion mock data



// #region unit tests
SCENARIO( "Showing code in a faction's style", "[CodeStyle]" ) {
	GIVEN( "a style for some governments" ) {
		const CodeStyle style = MakeStyle("\"code style\" Hai\n\tgovernment Hai \"Kor Efret\"\n"
			"\theader \"# hello\"\n\tcolor keyword 1 .5 0\n\tcipher zyxwvutsrqponmlkjihgfedcba\n");
		THEN( "it covers them and their variants, but not others" ) {
			CHECK( style.Name() == "Hai" );
			CHECK( style.Covers("Hai") );
			CHECK( style.Covers("Hai Merchant") );
			CHECK( style.Covers("Hai (Unfettered)") );
			CHECK( style.Covers("Kor Efret") );
			CHECK_FALSE( style.Covers("Haiku") );
			CHECK_FALSE( style.Covers("Korath") );
		}
		THEN( "it has its header and colors" ) {
			REQUIRE( style.Header().size() == 1 );
			CHECK( style.Header()[0] == "# hello" );
			REQUIRE( style.GetColor("keyword") );
			CHECK( style.GetColor("keyword")->Get()[1] == .5f );
			CHECK_FALSE( style.GetColor("name") );
		}
		THEN( "its cipher replaces letters, keeping case, numbers and symbols" ) {
			CHECK( style.HasCipher() );
			CHECK( style.Encode("set turn (* 2.5 Abz)") == "hvg gfim (* 2.5 Zya)" );
		}
	}
	GIVEN( "a style without a cipher" ) {
		const CodeStyle style = MakeStyle("\"code style\" Plain\n\tgovernment Merchant\n");
		THEN( "code shows as it is" ) {
			CHECK_FALSE( style.HasCipher() );
			CHECK( style.Encode("set turn 0") == "set turn 0" );
		}
	}
}
// #endregion unit tests



} // test namespace
