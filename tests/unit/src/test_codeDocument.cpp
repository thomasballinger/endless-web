/* test_codeDocument.cpp
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
#include "../../../source/CodeDocument.h"

#include <string>

namespace { // test namespace

// #region mock data
using Move = CodeDocument::Move;
using Position = CodeDocument::Position;
// #endregion mock data



// #region unit tests
SCENARIO( "Editing code", "[CodeDocument]" ) {
	GIVEN( "some text" ) {
		CodeDocument document("let a 1\nlet b 2");
		THEN( "it is split into lines, with the caret at the start" ) {
			REQUIRE( document.Lines().size() == 2 );
			CHECK( document.Lines()[1] == "let b 2" );
			CHECK( document.Caret() == Position{0, 0} );
			CHECK( document.Text() == "let a 1\nlet b 2" );
		}
		WHEN( "typing" ) {
			document.MoveCaret(Move::LINE_END, false);
			document.Insert("0");
			document.Insert("0");
			THEN( "the text appears at the caret" ) {
				CHECK( document.Lines()[0] == "let a 100" );
				CHECK( document.Caret() == Position{0, 9} );
			}
			THEN( "a run of typing is undone at once, and can be redone" ) {
				REQUIRE( document.Undo() );
				CHECK( document.Lines()[0] == "let a 1" );
				CHECK_FALSE( document.Undo() );
				REQUIRE( document.Redo() );
				CHECK( document.Lines()[0] == "let a 100" );
			}
		}
		WHEN( "typing over a selection" ) {
			document.Insert("x");
			document.SetCaret({1, 0}, false);
			document.MoveCaret(Move::LINE_END, true);
			document.Insert("s");
			document.Insert("et b 3");
			THEN( "one undo brings back what was selected" ) {
				CHECK( document.Lines()[1] == "set b 3" );
				REQUIRE( document.Undo() );
				CHECK( document.Lines()[1] == "let b 2" );
				CHECK( document.Lines()[0] == "xlet a 1" );
			}
		}
		WHEN( "pasting text with line breaks over a selection" ) {
			document.SetCaret({0, 4}, false);
			document.SetCaret({1, 5}, true);
			REQUIRE( document.SelectedText() == "a 1\nlet b" );
			document.Insert("x 1\nlet y");
			THEN( "it replaces the selection" ) {
				CHECK( document.Text() == "let x 1\nlet y 2" );
				CHECK( document.Caret() == Position{1, 5} );
				CHECK_FALSE( document.HasSelection() );
			}
		}
		WHEN( "deleting across lines" ) {
			document.SetCaret({1, 0}, false);
			document.Backspace();
			THEN( "the lines join" ) {
				CHECK( document.Text() == "let a 1let b 2" );
				CHECK( document.Caret() == Position{0, 7} );
			}
			document.Delete();
			THEN( "deleting forward removes the next character" ) {
				CHECK( document.Text() == "let a 1et b 2" );
			}
		}
	}
	GIVEN( "a line that starts a block" ) {
		CodeDocument document("guidance \"test\"\n\tif has-lock");
		document.SetCaret({1, 12}, false);
		WHEN( "starting a new line at its end" ) {
			document.NewLine();
			THEN( "the new line is indented one more" ) {
				CHECK( document.Lines()[2] == "\t\t" );
				CHECK( document.Caret() == Position{2, 2} );
			}
		}
		WHEN( "starting a new line in the middle of it" ) {
			document.SetCaret({1, 3}, false);
			document.NewLine();
			THEN( "the rest moves down, indented like the line" ) {
				CHECK( document.Lines()[1] == "\tif" );
				CHECK( document.Lines()[2] == "\t has-lock" );
			}
		}
	}
	GIVEN( "several lines selected" ) {
		CodeDocument document("a\n\tb\nc");
		document.SetCaret({0, 0}, false);
		document.SetCaret({2, 1}, true);
		WHEN( "indenting and unindenting" ) {
			document.Indent();
			THEN( "every selected line gets a tab" ) {
				CHECK( document.Text() == "\ta\n\t\tb\n\tc" );
			}
			document.Unindent();
			document.Unindent();
			THEN( "unindenting removes them, but only where there is indentation" ) {
				CHECK( document.Text() == "a\nb\nc" );
			}
		}
	}
	GIVEN( "moving the caret" ) {
		CodeDocument document("\tlet long-name 1\nx\n\tlet y (+ a b)");
		document.SetCaret({0, 12}, false);
		THEN( "up and down remember the column" ) {
			document.MoveCaret(Move::DOWN, false);
			CHECK( document.Caret() == Position{1, 1} );
			document.MoveCaret(Move::DOWN, false);
			CHECK( document.Caret() == Position{2, 12} );
		}
		THEN( "word movement skips names with dashes" ) {
			document.SetCaret({0, 5}, false);
			document.MoveCaret(Move::WORD_RIGHT, false);
			CHECK( document.Caret() == Position{0, 14} );
			document.MoveCaret(Move::WORD_LEFT, false);
			CHECK( document.Caret() == Position{0, 5} );
		}
		THEN( "home goes to the text, then to the start of the line" ) {
			document.MoveCaret(Move::LINE_START, false);
			CHECK( document.Caret() == Position{0, 1} );
			document.MoveCaret(Move::LINE_START, false);
			CHECK( document.Caret() == Position{0, 0} );
		}
		THEN( "shift-moving selects, and moving without it collapses the selection" ) {
			document.SetCaret({0, 14}, false);
			document.MoveCaret(Move::WORD_LEFT, true);
			CHECK( document.SelectedText() == "long-name" );
			document.MoveCaret(Move::LEFT, false);
			CHECK_FALSE( document.HasSelection() );
			CHECK( document.Caret() == Position{0, 5} );
		}
		THEN( "the version changes only when the text does" ) {
			const auto version = document.Version();
			document.MoveCaret(Move::DOCUMENT_END, false);
			CHECK( document.Version() == version );
			document.Insert("!");
			CHECK( document.Version() > version );
		}
	}
}
// #endregion unit tests



} // test namespace
