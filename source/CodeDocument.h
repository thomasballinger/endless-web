/* CodeDocument.h
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
#include <utility>
#include <vector>



// The text of a script being edited, with a caret, a selection, and undo
// history. It knows nothing about how it is drawn, so editors with different
// looks can share it.
class CodeDocument {
public:
	class Position {
	public:
		size_t line = 0;
		size_t column = 0;

		bool operator==(const Position &other) const = default;
		bool operator<(const Position &other) const;
	};

	enum class Move {
		LEFT, RIGHT, UP, DOWN, WORD_LEFT, WORD_RIGHT, LINE_START, LINE_END, DOCUMENT_START, DOCUMENT_END
	};


public:
	explicit CodeDocument(const std::string &text = "");

	// Replace all the text, forgetting the undo history.
	void SetText(const std::string &text);
	std::string Text() const;
	const std::vector<std::string> &Lines() const;
	// Increases every time the text changes.
	uint64_t Version() const;

	const Position &Caret() const;
	const Position &Anchor() const;
	bool HasSelection() const;
	// The start and end of the selection, in order.
	std::pair<Position, Position> Selection() const;
	std::string SelectedText() const;

	// Type the given text (which may have line breaks), replacing the selection.
	void Insert(const std::string &text);
	// Start a new line, indented like the current one, or one more if the
	// current line starts a block (e.g. "if" or "function").
	void NewLine();
	void Backspace();
	void Delete();
	// With a selection, indent or unindent every selected line by one tab;
	// without one, Indent() types a tab.
	void Indent();
	void Unindent();
	void SelectAll();

	// Move the caret, extending the selection if select is true. Moving up or
	// down remembers the column it started in.
	void MoveCaret(Move move, bool select, int count = 1);
	void SetCaret(Position position, bool select);

	bool Undo();
	bool Redo();


private:
	class State {
	public:
		std::vector<std::string> lines;
		Position caret;
		Position anchor;
	};
	enum class EditKind { OTHER, TYPING, DELETING };

	// Save the current state for undo before an edit. Runs of typing (or of
	// deleting) are undone together.
	void BeginEdit(EditKind kind);
	void DeleteSelection();
	Position Clamp(Position position) const;
	State Save() const;
	void Restore(const State &state);


private:
	std::vector<std::string> lines;
	Position caret;
	Position anchor;
	// The column to aim for when moving up and down.
	size_t preferredColumn = 0;
	uint64_t version = 0;

	std::vector<State> undo;
	std::vector<State> redo;
	EditKind lastEdit = EditKind::OTHER;
};
