/* CodeEditor.h
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

#include "CodeDocument.h"
#include "Point.h"
#include "Rectangle.h"

#include "SDL.h"

#include <map>
#include <string>
#include <vector>

class Color;



// Draws a CodeDocument as editable code, and turns keys, typed text, and the
// mouse into edits. It is not a Panel, so any panel can show one (or more)
// wherever it likes and pass it the events it should handle.
//
// Besides the code it can show, for each line, a mistake that keeps the script
// from compiling, and for a run of the script, which lines ran and what they
// produced.
class CodeEditor {
public:
	// What happened on one line during the run being shown.
	class LineNote {
	public:
		std::string text;
		bool isError = false;
	};


public:
	explicit CodeEditor(CodeDocument &document);

	void SetArea(const Rectangle &area);
	const Rectangle &Area() const;
	// Whether keys and typed text go to this editor.
	void SetFocus(bool focus);
	bool HasFocus() const;

	// Lines here count from 1, as in a data file made from the document.
	void SetErrors(const std::map<size_t, std::string> &errors);
	// Which lines ran, and a note to show beside each, or none to show none.
	void SetRun(const std::map<size_t, LineNote> &notes);

	void Draw() const;

	// Handle input. Each returns false if the editor did not use it.
	bool KeyDown(SDL_Keycode key, Uint16 mod);
	bool TextInput(const std::string &text);
	bool Click(const Point &point, int clicks);
	bool Drag(const Point &point);
	bool Scroll(double dy);


private:
	// The horizontal position of each column of a line, from the start of the
	// text (one more than the line's length).
	std::vector<double> ColumnX(const std::string &line) const;
	CodeDocument::Position PositionAt(const Point &point) const;
	// Keep the caret on screen.
	void ScrollToCaret();
	int VisibleLines() const;
	void DrawLine(const std::string &line, const Point &start, bool dim) const;


private:
	CodeDocument &document;
	Rectangle area;
	bool hasFocus = false;
	// The first line shown.
	int scroll = 0;
	std::map<size_t, std::string> errors;
	std::map<size_t, LineNote> notes;
	// For blinking the caret.
	mutable int blink = 0;
};
