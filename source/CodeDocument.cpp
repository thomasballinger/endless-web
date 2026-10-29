/* CodeDocument.cpp
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

#include "CodeDocument.h"

#include <algorithm>
#include <cctype>
#include <set>

using namespace std;

namespace {
	// How many undo steps to keep.
	const size_t MAX_UNDO = 200;

	// Lines starting with these words begin an indented block.
	const set<string> BLOCK_WORDS = {"if", "elif", "else", "function", "for", "guidance", "maneuver", "turret",
		"rate", "choose", "ai", "program"};

	bool IsWordChar(char c)
	{
		return isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.';
	}

	string Indentation(const string &line)
	{
		size_t end = 0;
		while(end < line.size() && (line[end] == '\t' || line[end] == ' '))
			++end;
		return line.substr(0, end);
	}
}



bool CodeDocument::Position::operator<(const Position &other) const
{
	return line < other.line || (line == other.line && column < other.column);
}



CodeDocument::CodeDocument(const string &text)
{
	SetText(text);
}



void CodeDocument::SetText(const string &text)
{
	lines.clear();
	size_t start = 0;
	while(true)
	{
		size_t end = text.find('\n', start);
		lines.push_back(text.substr(start, end == string::npos ? string::npos : end - start));
		if(end == string::npos)
			break;
		start = end + 1;
	}
	// Ignore carriage returns.
	for(string &line : lines)
		line.erase(remove(line.begin(), line.end(), '\r'), line.end());
	caret = anchor = Position();
	preferredColumn = 0;
	undo.clear();
	redo.clear();
	lastEdit = EditKind::OTHER;
	++version;
}



string CodeDocument::Text() const
{
	string text;
	for(size_t i = 0; i < lines.size(); ++i)
		text += (i ? "\n" : "") + lines[i];
	return text;
}



const vector<string> &CodeDocument::Lines() const
{
	return lines;
}



uint64_t CodeDocument::Version() const
{
	return version;
}



const CodeDocument::Position &CodeDocument::Caret() const
{
	return caret;
}



const CodeDocument::Position &CodeDocument::Anchor() const
{
	return anchor;
}



bool CodeDocument::HasSelection() const
{
	return !(caret == anchor);
}



pair<CodeDocument::Position, CodeDocument::Position> CodeDocument::Selection() const
{
	return caret < anchor ? make_pair(caret, anchor) : make_pair(anchor, caret);
}



string CodeDocument::SelectedText() const
{
	auto [start, end] = Selection();
	if(start.line == end.line)
		return lines[start.line].substr(start.column, end.column - start.column);
	string text = lines[start.line].substr(start.column);
	for(size_t i = start.line + 1; i < end.line; ++i)
		text += "\n" + lines[i];
	return text + "\n" + lines[end.line].substr(0, end.column);
}



void CodeDocument::Insert(const string &text)
{
	if(text.empty())
		return;
	BeginEdit(text.find('\n') == string::npos && !HasSelection() ? EditKind::TYPING : EditKind::OTHER);
	DeleteSelection();
	string after = lines[caret.line].substr(caret.column);
	lines[caret.line].erase(caret.column);
	size_t start = 0;
	while(true)
	{
		size_t end = text.find('\n', start);
		string piece = text.substr(start, end == string::npos ? string::npos : end - start);
		piece.erase(remove(piece.begin(), piece.end(), '\r'), piece.end());
		lines[caret.line] += piece;
		caret.column = lines[caret.line].size();
		if(end == string::npos)
			break;
		lines.insert(lines.begin() + caret.line + 1, string());
		++caret.line;
		caret.column = 0;
		start = end + 1;
	}
	lines[caret.line] += after;
	anchor = caret;
	preferredColumn = caret.column;
	++version;
}



void CodeDocument::NewLine()
{
	const string &line = lines[caret.line];
	string indent = Indentation(line);
	// Only indent more if the caret is at the end of a line that starts a block.
	size_t wordEnd = indent.size();
	while(wordEnd < line.size() && !isspace(static_cast<unsigned char>(line[wordEnd])))
		++wordEnd;
	if(!HasSelection() && caret.column == line.size()
			&& BLOCK_WORDS.contains(line.substr(indent.size(), wordEnd - indent.size())))
		indent += '\t';
	BeginEdit(EditKind::OTHER);
	lastEdit = EditKind::OTHER;
	// Insert() would start a new undo step; this one is already saved.
	DeleteSelection();
	string after = lines[caret.line].substr(caret.column);
	lines[caret.line].erase(caret.column);
	lines.insert(lines.begin() + caret.line + 1, indent + after);
	++caret.line;
	caret.column = indent.size();
	anchor = caret;
	preferredColumn = caret.column;
	++version;
}



void CodeDocument::Backspace()
{
	if(HasSelection())
	{
		BeginEdit(EditKind::OTHER);
		DeleteSelection();
		++version;
		return;
	}
	if(!caret.line && !caret.column)
		return;
	BeginEdit(EditKind::DELETING);
	if(caret.column)
	{
		lines[caret.line].erase(caret.column - 1, 1);
		--caret.column;
	}
	else
	{
		caret.column = lines[caret.line - 1].size();
		lines[caret.line - 1] += lines[caret.line];
		lines.erase(lines.begin() + caret.line);
		--caret.line;
	}
	anchor = caret;
	preferredColumn = caret.column;
	++version;
}



void CodeDocument::Delete()
{
	if(HasSelection())
	{
		BeginEdit(EditKind::OTHER);
		DeleteSelection();
		++version;
		return;
	}
	if(caret.line + 1 == lines.size() && caret.column == lines[caret.line].size())
		return;
	BeginEdit(EditKind::DELETING);
	if(caret.column < lines[caret.line].size())
		lines[caret.line].erase(caret.column, 1);
	else
	{
		lines[caret.line] += lines[caret.line + 1];
		lines.erase(lines.begin() + caret.line + 1);
	}
	anchor = caret;
	++version;
}



void CodeDocument::Indent()
{
	if(!HasSelection())
	{
		Insert("\t");
		return;
	}
	BeginEdit(EditKind::OTHER);
	auto [start, end] = Selection();
	// A selection ending at the start of a line doesn't include that line.
	size_t last = (end.column || end.line == start.line) ? end.line : end.line - 1;
	for(size_t i = start.line; i <= last; ++i)
		lines[i].insert(0, "\t");
	caret = {caret.line, caret.column + (caret.line <= last ? 1 : 0)};
	anchor = {anchor.line, anchor.column + (anchor.line <= last ? 1 : 0)};
	++version;
}



void CodeDocument::Unindent()
{
	auto [start, end] = Selection();
	size_t last = (end.column || end.line == start.line) ? end.line : end.line - 1;
	bool any = false;
	for(size_t i = start.line; i <= last; ++i)
		any |= (!lines[i].empty() && (lines[i][0] == '\t' || lines[i][0] == ' '));
	if(!any)
		return;
	BeginEdit(EditKind::OTHER);
	for(size_t i = start.line; i <= last; ++i)
		if(!lines[i].empty() && (lines[i][0] == '\t' || lines[i][0] == ' '))
		{
			lines[i].erase(0, 1);
			if(caret.line == i && caret.column)
				--caret.column;
			if(anchor.line == i && anchor.column)
				--anchor.column;
		}
	++version;
}



void CodeDocument::SelectAll()
{
	anchor = Position();
	caret = {lines.size() - 1, lines.back().size()};
}



void CodeDocument::MoveCaret(Move move, bool select, int count)
{
	// Without extending it, moving left or right from a selection goes to
	// its start or end.
	if(!select && HasSelection() && (move == Move::LEFT || move == Move::RIGHT))
	{
		caret = (move == Move::LEFT) ? Selection().first : Selection().second;
		anchor = caret;
		preferredColumn = caret.column;
		return;
	}
	for(int i = 0; i < count; ++i)
	{
		const string &line = lines[caret.line];
		switch(move)
		{
			case Move::LEFT:
				if(caret.column)
					--caret.column;
				else if(caret.line)
					caret = {caret.line - 1, lines[caret.line - 1].size()};
				break;
			case Move::RIGHT:
				if(caret.column < line.size())
					++caret.column;
				else if(caret.line + 1 < lines.size())
					caret = {caret.line + 1, 0};
				break;
			case Move::UP:
				if(caret.line)
					caret = {caret.line - 1, min(preferredColumn, lines[caret.line - 1].size())};
				else
					caret.column = 0;
				break;
			case Move::DOWN:
				if(caret.line + 1 < lines.size())
					caret = {caret.line + 1, min(preferredColumn, lines[caret.line + 1].size())};
				else
					caret.column = line.size();
				break;
			case Move::WORD_LEFT:
				if(!caret.column && caret.line)
					caret = {caret.line - 1, lines[caret.line - 1].size()};
				else
				{
					while(caret.column && !IsWordChar(line[caret.column - 1]))
						--caret.column;
					while(caret.column && IsWordChar(line[caret.column - 1]))
						--caret.column;
				}
				break;
			case Move::WORD_RIGHT:
				if(caret.column == line.size() && caret.line + 1 < lines.size())
					caret = {caret.line + 1, 0};
				else
				{
					while(caret.column < line.size() && !IsWordChar(line[caret.column]))
						++caret.column;
					while(caret.column < line.size() && IsWordChar(line[caret.column]))
						++caret.column;
				}
				break;
			case Move::LINE_START:
			{
				// Go to the start of the text, or if already there, to the
				// start of the line.
				size_t indent = Indentation(line).size();
				caret.column = (caret.column == indent) ? 0 : indent;
				break;
			}
			case Move::LINE_END:
				caret.column = line.size();
				break;
			case Move::DOCUMENT_START:
				caret = Position();
				break;
			case Move::DOCUMENT_END:
				caret = {lines.size() - 1, lines.back().size()};
				break;
		}
	}
	if(move != Move::UP && move != Move::DOWN)
		preferredColumn = caret.column;
	if(!select)
		anchor = caret;
	lastEdit = EditKind::OTHER;
}



void CodeDocument::SetCaret(Position position, bool select)
{
	caret = Clamp(position);
	if(!select)
		anchor = caret;
	preferredColumn = caret.column;
	lastEdit = EditKind::OTHER;
}



bool CodeDocument::Undo()
{
	if(undo.empty())
		return false;
	redo.push_back(Save());
	Restore(undo.back());
	undo.pop_back();
	return true;
}



bool CodeDocument::Redo()
{
	if(redo.empty())
		return false;
	undo.push_back(Save());
	Restore(redo.back());
	redo.pop_back();
	return true;
}



void CodeDocument::BeginEdit(EditKind kind)
{
	redo.clear();
	// Continue the current undo step while typing (or deleting) runs on.
	if(kind != EditKind::OTHER && kind == lastEdit && !undo.empty())
		return;
	undo.push_back(Save());
	if(undo.size() > MAX_UNDO)
		undo.erase(undo.begin());
	lastEdit = kind;
}



void CodeDocument::DeleteSelection()
{
	if(!HasSelection())
		return;
	auto [start, end] = Selection();
	lines[start.line] = lines[start.line].substr(0, start.column) + lines[end.line].substr(end.column);
	lines.erase(lines.begin() + start.line + 1, lines.begin() + end.line + 1);
	caret = anchor = start;
	preferredColumn = caret.column;
}



CodeDocument::Position CodeDocument::Clamp(Position position) const
{
	position.line = min(position.line, lines.size() - 1);
	position.column = min(position.column, lines[position.line].size());
	return position;
}



CodeDocument::State CodeDocument::Save() const
{
	return {lines, caret, anchor};
}



void CodeDocument::Restore(const State &state)
{
	lines = state.lines;
	caret = state.caret;
	anchor = state.anchor;
	preferredColumn = caret.column;
	lastEdit = EditKind::OTHER;
	++version;
}
