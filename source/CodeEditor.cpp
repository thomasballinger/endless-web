/* CodeEditor.cpp
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

#include "CodeEditor.h"

#include "CodeStyle.h"
#include "Color.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "shader/LineShader.h"
#include "shader/PointerShader.h"
#include "script/Script.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {
	const double LINE_HEIGHT = 18.;
	const double GUTTER = 40.;
	const double PADDING = 8.;
	// How wide a tab is drawn.
	const double TAB_WIDTH = 20.;

	const Color BACKGROUND(.01f, .035f, .02f, 1.f);
	const Color GUTTER_COLOR(0.f, .02f, .01f, 1.f);
	const Color CURRENT_LINE(.04f, .09f, .06f, .9f);
	const Color SELECTION(.10f, .25f, .40f, .9f);
	const Color CARET(.90f, 1.f, .92f, 1.f);
	const Color LINE_NUMBER(.30f, .45f, .35f, 1.f);
	const Color RAN(.30f, .85f, .45f, 1.f);
	const Color ERROR_COLOR(.95f, .35f, .30f, 1.f);
	const Color NOTE(.95f, .82f, .40f, 1.f);
	const Color GLITCH(.95f, .40f, .95f, 1.f);
	// What glitching code turns into.
	const string NOISE = "#%&@$*=+<>/\\|?!~^01";
	// How many frames a glitch pattern lasts.
	const int GLITCH_FRAMES = 4;

	const set<string> KEYWORDS = {"let", "set", "if", "elif", "else", "memory", "stop", "function", "return",
		"for", "in"};

	enum class TokenKind { SPACE, COMMENT, STRING, NUMBER, PAREN, KEYWORD, NAME };

	class Token {
	public:
		TokenKind kind;
		string text;
	};

	// Split a line into pieces to color.
	vector<Token> Tokenize(const string &line)
	{
		vector<Token> tokens;
		size_t i = 0;
		while(i < line.size())
		{
			const char c = line[i];
			size_t end = i + 1;
			TokenKind kind = TokenKind::NAME;
			if(c == ' ' || c == '\t')
			{
				kind = TokenKind::SPACE;
				end = i + 1;
			}
			else if(c == '#')
			{
				kind = TokenKind::COMMENT;
				end = line.size();
			}
			else if(c == '"' || c == '`')
			{
				kind = TokenKind::STRING;
				end = line.find(c, i + 1);
				end = (end == string::npos) ? line.size() : end + 1;
			}
			else if(c == '(' || c == ')')
				kind = TokenKind::PAREN;
			else
			{
				while(end < line.size() && !strchr(" \t()#\"`", line[end]))
					++end;
				const string word = line.substr(i, end - i);
				if(KEYWORDS.contains(word))
					kind = TokenKind::KEYWORD;
				else if(isdigit(static_cast<unsigned char>(c)) || ((c == '.' || c == '-') && word.size() > 1
						&& (isdigit(static_cast<unsigned char>(word[1])) || word[1] == '.')))
					kind = TokenKind::NUMBER;
			}
			tokens.push_back({kind, line.substr(i, end - i)});
			i = end;
		}
		return tokens;
	}

	Color TokenColor(TokenKind kind, bool dim, const CodeStyle *style)
	{
		static const map<TokenKind, string> PARTS = {{TokenKind::KEYWORD, "keyword"},
			{TokenKind::NUMBER, "number"}, {TokenKind::COMMENT, "comment"}, {TokenKind::STRING, "string"},
			{TokenKind::PAREN, "paren"}, {TokenKind::NAME, "name"}};
		Color color;
		auto part = PARTS.find(kind);
		const Color *styled = (style && part != PARTS.end()) ? style->GetColor(part->second) : nullptr;
		if(styled)
			color = *styled;
		else
			switch(kind)
			{
				case TokenKind::KEYWORD:
					color = Color(.55f, .80f, 1.f, 1.f);
					break;
				case TokenKind::NUMBER:
					color = Color(.95f, .72f, .45f, 1.f);
					break;
				case TokenKind::COMMENT:
					color = Color(.42f, .55f, .48f, 1.f);
					break;
				case TokenKind::STRING:
					color = Color(.70f, .95f, .60f, 1.f);
					break;
				case TokenKind::PAREN:
					color = Color(.55f, .68f, .60f, 1.f);
					break;
				default:
					color = Color(.86f, .93f, .88f, 1.f);
			}
		if(dim)
			color = Color(color.Get()[0] * .55f, color.Get()[1] * .55f, color.Get()[2] * .55f, .55f);
		return color;
	}

	// Tabs inside a line are shown as spaces.
	string Displayed(string text)
	{
		replace(text.begin(), text.end(), '\t', ' ');
		return text;
	}



	// A number that looks random but depends only on its arguments, so that
	// glitching never takes numbers from the game's random sequences.
	uint32_t Hash(uint32_t a, uint32_t b, uint32_t c)
	{
		uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u;
		h = h ^ (h >> 15);
		h *= 0x2c1b3c6du;
		h = h ^ (h >> 12);
		h *= 0x297a2d39u;
		return h ^ (h >> 15);
	}

	bool IsWordChar(char c)
	{
		return isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.';
	}
}



CodeEditor::CodeEditor(CodeDocument &document)
	: document(document)
{
}



void CodeEditor::SetArea(const Rectangle &area)
{
	this->area = area;
}



const Rectangle &CodeEditor::Area() const
{
	return area;
}



void CodeEditor::SetFocus(bool focus)
{
	if(focus == hasFocus)
		return;
	hasFocus = focus;
#ifdef ES_USE_SDL3
	if(focus)
		SDL_StartTextInput(SDL_GetKeyboardFocus());
	else
		SDL_StopTextInput(SDL_GetKeyboardFocus());
#else
	if(focus)
		SDL_StartTextInput();
	else
		SDL_StopTextInput();
#endif
}



bool CodeEditor::HasFocus() const
{
	return hasFocus;
}



void CodeEditor::SetErrors(const map<size_t, string> &errors)
{
	this->errors = errors;
}



void CodeEditor::SetRun(const map<size_t, LineNote> &notes)
{
	this->notes = notes;
}



map<size_t, CodeEditor::LineNote> CodeEditor::RunNotes(const script::Trace &trace, size_t firstLine, size_t above)
{
	// Numbers, with fewer digits than a script would print, and vectors as
	// arrows (with their length) rather than as numbers.
	auto number = [](double value) {
		char buffer[32];
		snprintf(buffer, sizeof(buffer), fabs(value) >= 1000. ? "%.0f" : "%.3g", value);
		return string(buffer);
	};
	auto show = [&number](const script::Value &value) {
		if(value.GetType() == script::Value::Type::NUMBER)
			return number(value.Number());
		if(value.GetType() == script::Value::Type::VECTOR)
			return number(value.Vector().Length());
		return value.ToString();
	};
	map<size_t, LineNote> notes;
	auto line = [firstLine, above](size_t fileLine) { return fileLine + 1 - firstLine + above; };
	using Kind = script::Trace::Step::Kind;
	for(const script::Trace::Step &step : trace.steps)
	{
		if(step.line < firstLine)
			continue;
		string text;
		switch(step.kind)
		{
			case Kind::ASSIGN:
				text = step.name + " = " + show(step.value);
				break;
			case Kind::CONDITION:
				text = show(step.value);
				break;
			case Kind::EFFECT:
				text = step.name;
				break;
			case Kind::STOP:
				text = "stopped";
				break;
			case Kind::RETURN:
				text = "returned " + show(step.value);
				break;
			case Kind::LOOP:
				text = "#" + step.value.ToString();
				break;
			case Kind::WAIT:
				text = "waiting " + step.value.ToString() + (step.value.Number() == 1. ? " step" : " steps");
				break;
			case Kind::CALL:
				text = "running " + step.name;
				break;
		}
		LineNote &note = notes[line(step.line)];
		note = {text, false};
		if(step.kind != Kind::LOOP && step.value.GetType() == script::Value::Type::VECTOR)
		{
			note.isVector = true;
			note.vector = step.value.Vector();
		}
	}
	if(!trace.error.empty() && trace.errorLine >= firstLine)
		notes[line(trace.errorLine)] = {"error: " + trace.error, true};
	return notes;
}



void CodeEditor::SetStyle(const CodeStyle *style)
{
	this->style = style;
}



void CodeEditor::SetGlitch(const string &message, bool scramble)
{
	glitch = message;
	this->scramble = scramble;
}



void CodeEditor::SetCompletions(const vector<string> &names)
{
	completions = names;
}



string CodeEditor::WordBeforeCaret() const
{
	const CodeDocument::Position &caret = document.Caret();
	if(document.HasSelection() || caret.line >= document.Lines().size())
		return "";
	const string &line = document.Lines()[caret.line];
	size_t start = min(caret.column, line.size());
	while(start > 0 && (isalnum(static_cast<unsigned char>(line[start - 1])) || line[start - 1] == '_'
			|| line[start - 1] == '.'))
		--start;
	// Not in a comment, and not a number.
	if(line.find('#') < start)
		return "";
	const string word = line.substr(start, caret.column - start);
	if(word.empty() || isdigit(static_cast<unsigned char>(word[0])))
		return "";
	return word;
}



vector<string> CodeEditor::Suggestions() const
{
	const string word = WordBeforeCaret();
	if(word.empty())
		return {};
	// The names given, and the ones the code defines.
	vector<string> names = completions;
	for(const string &line : document.Lines())
	{
		istringstream in(line);
		string keyword;
		string name;
		in >> keyword >> name;
		if(keyword == "let" || keyword == "memory" || keyword == "function")
			names.push_back(name.substr(0, name.find('(')));
	}
	vector<string> matches;
	for(const string &name : names)
		if(name.size() > word.size() && !name.compare(0, word.size(), word)
				&& find(matches.begin(), matches.end(), name) == matches.end())
			matches.push_back(name);
	// The shortest first, as the likeliest to be meant.
	stable_sort(matches.begin(), matches.end(), [](const string &a, const string &b) { return a.size() < b.size(); });
	if(word != completionWord)
	{
		completionWord = word;
		completionIndex = 0;
	}
	if(completionIndex >= matches.size())
		completionIndex = 0;
	return matches;
}



void CodeEditor::DrawSuggestions() const
{
	const vector<string> matches = Suggestions();
	if(matches.empty())
		return;
	const Font &font = FontSet::Get(14);
	const CodeDocument::Position &caret = document.Caret();
	const int row = static_cast<int>(caret.line) - scroll;
	if(row < 0 || row >= VisibleLines())
		return;
	const vector<double> columns = ColumnX(document.Lines()[caret.line]);
	const double x = area.Left() + GUTTER + PADDING + columns[min(caret.column, columns.size() - 1)]
		- font.Width(completionWord);
	double y = area.Top() + PADDING + (row + 1) * LINE_HEIGHT + 2.;
	const size_t shown = min<size_t>(6, matches.size());
	double width = 0.;
	for(size_t i = 0; i < shown; ++i)
		width = max<double>(width, font.Width(matches[i]));
	const Rectangle box = Rectangle::WithCorners(Point(x - 4., y - 2.),
		Point(x + width + 8., y + shown * LINE_HEIGHT + 2.));
	FillShader::Fill(box.Center(), box.Dimensions(), Color(.02f, .10f, .06f, .97f));
	for(size_t i = 0; i < shown; ++i)
	{
		if(i == completionIndex)
			FillShader::Fill(Point(box.Center().X(), y + LINE_HEIGHT / 2.), Point(box.Width(), LINE_HEIGHT), SELECTION);
		font.DrawCode(matches[i], Point(x, y + 1.), {i == completionIndex ? CARET : LINE_NUMBER});
		y += LINE_HEIGHT;
	}
}



void CodeEditor::SetDocumentation(const map<string, pair<string, string>> &docs)
{
	this->docs = docs;
}



string CodeEditor::WordAtCaret() const
{
	const CodeDocument::Position &caret = document.Caret();
	if(caret.line >= document.Lines().size())
		return "";
	const string &line = document.Lines()[caret.line];
	auto isName = [&line](size_t i) {
		return isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_' || line[i] == '.';
	};
	size_t start = min(caret.column, line.size());
	size_t end = start;
	while(start > 0 && isName(start - 1))
		--start;
	while(end < line.size() && isName(end))
		++end;
	if(line.find('#') < start)
		return "";
	return line.substr(start, end - start);
}



void CodeEditor::DrawDocumentation() const
{
	const auto it = docs.find(WordAtCaret());
	if(it == docs.end())
		return;
	// A box across the bottom of the editor: the name, what it is, and how
	// it is worked out, wrapped to fit.
	const Font &font = FontSet::Get(14);
	const double width = area.Width() - 24.;
	auto wrap = [&font, width](const string &text) {
		vector<string> lines(1);
		istringstream in(text);
		string word;
		while(in >> word)
		{
			const string candidate = lines.back().empty() ? word : lines.back() + " " + word;
			if(!lines.back().empty() && font.Width(candidate) > width)
				lines.push_back(word);
			else
				lines.back() = candidate;
		}
		return lines;
	};
	// (A function's description already starts with how to call it.)
	const bool named = !it->second.first.compare(0, it->first.size() + 1, it->first + "(");
	const vector<string> text = wrap(named ? it->second.first : it->first + ": " + it->second.first);
	const vector<string> source = it->second.second.empty() ? vector<string>() : wrap("is " + it->second.second);
	const double height = (text.size() + source.size()) * LINE_HEIGHT + 12.;
	const double top = area.Bottom() - height - 4.;
	FillShader::Fill(Point(area.Center().X(), top + height / 2.), Point(area.Width() - 8., height),
		Color(.02f, .08f, .05f, .97f));
	double y = top + 6.;
	for(const string &line : text)
	{
		font.DrawCode(line, Point(area.Left() + 12., y), {NOTE});
		y += LINE_HEIGHT;
	}
	for(const string &line : source)
	{
		font.DrawCode(line, Point(area.Left() + 12., y), {LINE_NUMBER});
		y += LINE_HEIGHT;
	}
}



void CodeEditor::SetCaption(const string &caption, const Color &color)
{
	this->caption = caption;
	captionColor.assign(color.Get(), color.Get() + 4);
}



void CodeEditor::Draw() const
{
	const Font &font = FontSet::Get(14);
	const Color *background = style ? style->GetColor("background") : nullptr;
	FillShader::Fill(area.Center(), area.Dimensions(), background ? *background : BACKGROUND);
	const Color gutter = background ? Color(background->Get()[0] * .6f, background->Get()[1] * .6f,
		background->Get()[2] * .6f, 1.f) : GUTTER_COLOR;
	FillShader::Fill(Point(area.Left() + GUTTER / 2., area.Center().Y()), Point(GUTTER, area.Height()), gutter);

	const vector<string> &lines = document.Lines();
	const auto [selectionStart, selectionEnd] = document.Selection();
	const double textLeft = area.Left() + GUTTER + PADDING;
	const int visible = VisibleLines();
	for(int i = scroll; i < static_cast<int>(lines.size()) && i < scroll + visible; ++i)
	{
		const size_t lineNumber = i + 1;
		const double y = area.Top() + PADDING + (i - scroll) * LINE_HEIGHT;
		const string &line = lines[i];
		vector<double> columns;
		const bool isCaretLine = (static_cast<size_t>(i) == document.Caret().line);
		if(isCaretLine && hasFocus)
			FillShader::Fill(Point(area.Center().X() + GUTTER / 2., y + LINE_HEIGHT / 2.),
				Point(area.Width() - GUTTER, LINE_HEIGHT), CURRENT_LINE);

		// The selection on this line.
		if(document.HasSelection() && selectionStart.line <= static_cast<size_t>(i)
				&& static_cast<size_t>(i) <= selectionEnd.line)
		{
			columns = ColumnX(line);
			const size_t from = (selectionStart.line == static_cast<size_t>(i)) ? selectionStart.column : 0;
			const size_t to = (selectionEnd.line == static_cast<size_t>(i)) ? selectionEnd.column : line.size();
			double left = textLeft + columns[from];
			double right = textLeft + columns[to] + (selectionEnd.line != static_cast<size_t>(i) ? 6. : 0.);
			if(right > left)
				FillShader::Fill(Point((left + right) / 2., y + LINE_HEIGHT / 2.), Point(right - left, LINE_HEIGHT),
					SELECTION);
		}

		// The gutter: the line number, marked if the line ran or has a mistake.
		const auto error = errors.find(lineNumber);
		const auto note = notes.find(lineNumber);
		const string number = to_string(lineNumber);
		font.Draw(number, Point(area.Left() + GUTTER - 6. - font.Width(number), y + 1.),
			error != errors.end() ? ERROR_COLOR : LINE_NUMBER);
		if(note != notes.end())
			FillShader::Fill(Point(area.Left() + GUTTER - 2., y + LINE_HEIGHT / 2.), Point(3., LINE_HEIGHT - 2.),
				note->second.isError ? ERROR_COLOR : RAN);

		// While showing a run, dim the lines that didn't run.
		const bool dim = !notes.empty() && note == notes.end();
		DrawLine(line, Point(textLeft, y + 1.), dim, i);

		// What the line did, or what is wrong with it.
		string comment;
		Color commentColor = NOTE;
		if(error != errors.end())
		{
			comment = error->second;
			commentColor = ERROR_COLOR;
		}
		else if(note != notes.end())
		{
			comment = note->second.text;
			if(note->second.isError)
				commentColor = ERROR_COLOR;
		}
		if(!comment.empty())
		{
			if(columns.empty())
				columns = ColumnX(line);
			const double x = max(textLeft + columns.back() + 18., textLeft + 150.);
			if(x < area.Right() - 20.)
			{
				const bool arrow = (error == errors.end() && note != notes.end() && note->second.isVector
					&& !style);
				// A vector's arrow goes after the "=", before its length.
				string before = comment;
				string after;
				const size_t equals = comment.find(" = ");
				if(arrow)
				{
					before = (equals == string::npos) ? string() : comment.substr(0, equals + 3);
					after = (equals == string::npos) ? comment : comment.substr(equals + 3);
				}
				font.DrawCode(style ? style->Encode(before) : before, Point(x, y + 1.), {commentColor});
				if(arrow)
				{
					const Point center(x + font.Width(before) + 9., y + LINE_HEIGHT / 2.);
					const Point vector = note->second.vector;
					if(vector)
					{
						const Point unit = vector.Unit();
						LineShader::Draw(center - unit * 7., center + unit * 4., 1.5f, commentColor, false);
						PointerShader::Draw(center + unit * 4., unit, 7.f, 6.f, 0.f, commentColor);
					}
					else
						FillShader::Fill(center, Point(3., 3.), commentColor);
					font.DrawCode(after, Point(center.X() + 12., y + 1.), {commentColor});
				}
			}
		}

		if(isCaretLine && hasFocus && (blink / 30) % 2 == 0)
		{
			if(columns.empty())
				columns = ColumnX(line);
			const double x = textLeft + columns[min(document.Caret().column, line.size())];
			FillShader::Fill(Point(x, y + LINE_HEIGHT / 2.), Point(2., LINE_HEIGHT - 2.), CARET);
		}
	}
	if(!caption.empty())
	{
		const Font &small = FontSet::Get(14);
		const Color color(captionColor[0], captionColor[1], captionColor[2], captionColor[3]);
		const Point corner(area.Right() - small.Width(caption) - 10., area.Top() + 6.);
		FillShader::Fill(corner + Point(small.Width(caption) / 2., 8.), Point(small.Width(caption) + 12., 20.),
			background ? *background : BACKGROUND);
		small.Draw(caption, corner, color);
	}
	if(!glitch.empty())
		DrawGlitchMessage();
	if(hasFocus)
	{
		DrawDocumentation();
		DrawSuggestions();
	}
	++blink;
	// A border shows which editor has the keyboard.
	if(hasFocus)
	{
		const Color border(.25f, .65f, .40f, 1.f);
		LineShader::Draw(area.TopLeft(), Point(area.Right(), area.Top()), 1.f, border, false);
		LineShader::Draw(Point(area.Left(), area.Bottom()), Point(area.Right(), area.Bottom()), 1.f, border, false);
		LineShader::Draw(area.TopLeft(), Point(area.Left(), area.Bottom()), 1.f, border, false);
		LineShader::Draw(Point(area.Right(), area.Top()), Point(area.Right(), area.Bottom()), 1.f, border, false);
	}
}



bool CodeEditor::KeyDown(SDL_Keycode key, Uint16 mod)
{
	if(!hasFocus)
		return false;
	blink = 0;
	const bool shift = mod & KMOD_SHIFT;
	const bool control = mod & (KMOD_CTRL | KMOD_GUI);
	using Move = CodeDocument::Move;
	// While there are suggestions, tab takes one, and up and down choose.
	if(!control && !shift && (key == SDLK_TAB || key == SDLK_UP || key == SDLK_DOWN))
	{
		const vector<string> matches = Suggestions();
		if(!matches.empty())
		{
			if(key == SDLK_TAB)
				document.Insert(matches[completionIndex].substr(completionWord.size()));
			else
				completionIndex = (completionIndex + (key == SDLK_DOWN ? 1 : matches.size() - 1)) % matches.size();
			ScrollToCaret();
			return true;
		}
	}
	switch(key)
	{
		case SDLK_LEFT:
			document.MoveCaret(control ? Move::WORD_LEFT : Move::LEFT, shift);
			break;
		case SDLK_RIGHT:
			document.MoveCaret(control ? Move::WORD_RIGHT : Move::RIGHT, shift);
			break;
		case SDLK_UP:
			document.MoveCaret(Move::UP, shift);
			break;
		case SDLK_DOWN:
			document.MoveCaret(Move::DOWN, shift);
			break;
		case SDLK_HOME:
			document.MoveCaret(control ? Move::DOCUMENT_START : Move::LINE_START, shift);
			break;
		case SDLK_END:
			document.MoveCaret(control ? Move::DOCUMENT_END : Move::LINE_END, shift);
			break;
		case SDLK_PAGEUP:
			document.MoveCaret(Move::UP, shift, max(1, VisibleLines() - 2));
			break;
		case SDLK_PAGEDOWN:
			document.MoveCaret(Move::DOWN, shift, max(1, VisibleLines() - 2));
			break;
		case SDLK_BACKSPACE:
			document.Backspace();
			break;
		case SDLK_DELETE:
			document.Delete();
			break;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			document.NewLine();
			break;
		case SDLK_TAB:
			if(shift)
				document.Unindent();
			else
				document.Indent();
			break;
		default:
			if(!control)
			{
				// Printable keys arrive as text input; claim them so that
				// they don't also act as shortcuts.
				return key >= SDLK_SPACE && key < SDLK_DELETE;
			}
			if(key == SDLK_a)
				document.SelectAll();
			else if(key == SDLK_z && shift)
				document.Redo();
			else if(key == SDLK_z)
				document.Undo();
			else if(key == SDLK_y)
				document.Redo();
			else if((key == SDLK_c || key == SDLK_x) && document.HasSelection())
			{
				SDL_SetClipboardText(document.SelectedText().c_str());
				if(key == SDLK_x)
					document.Backspace();
			}
			else if(key == SDLK_v)
			{
				shared_ptr<char> text(SDL_GetClipboardText(), SDL_free);
				if(text)
					document.Insert(text.get());
			}
			else
				return false;
	}
	ScrollToCaret();
	return true;
}



bool CodeEditor::TextInput(const string &text)
{
	if(!hasFocus)
		return false;
	blink = 0;
	document.Insert(text);
	ScrollToCaret();
	return true;
}



bool CodeEditor::Click(const Point &point, int clicks)
{
	if(!area.Contains(point))
		return false;
	const CodeDocument::Position position = PositionAt(point);
	document.SetCaret(position, false);
	// Double-click a word to select it.
	if(clicks == 2)
	{
		const string &line = document.Lines()[position.line];
		size_t start = position.column;
		size_t end = position.column;
		while(start && IsWordChar(line[start - 1]))
			--start;
		while(end < line.size() && IsWordChar(line[end]))
			++end;
		document.SetCaret({position.line, start}, false);
		document.SetCaret({position.line, end}, true);
	}
	blink = 0;
	return true;
}



bool CodeEditor::Drag(const Point &point)
{
	document.SetCaret(PositionAt(point), true);
	ScrollToCaret();
	return true;
}



bool CodeEditor::Scroll(double dy)
{
	const int maxScroll = max(0, static_cast<int>(document.Lines().size()) - VisibleLines() + 2);
	scroll = max(0, min(maxScroll, scroll - static_cast<int>(round(dy * 3.))));
	return true;
}



vector<double> CodeEditor::ColumnX(const string &line) const
{
	// Leading tabs are drawn a fixed width; the rest is measured as one piece
	// of text, so that the font's spacing between letters is kept.
	const Font &font = FontSet::Get(14);
	vector<double> columns;
	columns.reserve(line.size() + 1);
	double x = 0.;
	size_t indent = 0;
	while(indent < line.size() && (line[indent] == '\t' || line[indent] == ' '))
	{
		columns.push_back(x);
		x += (line[indent] == '\t') ? TAB_WIDTH : font.CodeWidth(" ");
		++indent;
	}
	const string rest = Displayed(line.substr(indent));
	for(size_t k = 0; k <= rest.size(); ++k)
		columns.push_back(x + (k ? font.CodeWidth(rest.substr(0, k), k < rest.size() ? rest[k] : ' ') : 0.));
	return columns;
}



const CodeEditor::LineNote *CodeEditor::NoteAt(const Point &point) const
{
	if(!area.Contains(point) || point.X() < area.Left() + GUTTER)
		return nullptr;
	const int line = scroll + static_cast<int>(floor((point.Y() - area.Top() - PADDING) / LINE_HEIGHT));
	const auto it = notes.find(line + 1);
	return it == notes.end() ? nullptr : &it->second;
}



CodeDocument::Position CodeEditor::PositionAt(const Point &point) const
{
	const vector<string> &lines = document.Lines();
	int line = scroll + static_cast<int>(floor((point.Y() - area.Top() - PADDING) / LINE_HEIGHT));
	line = max(0, min(static_cast<int>(lines.size()) - 1, line));
	const vector<double> columns = ColumnX(lines[line]);
	const double x = point.X() - (area.Left() + GUTTER + PADDING);
	size_t column = 0;
	for(size_t i = 1; i < columns.size(); ++i)
		if(fabs(columns[i] - x) < fabs(columns[column] - x))
			column = i;
	return {static_cast<size_t>(line), column};
}



void CodeEditor::ScrollToCaret()
{
	const int line = static_cast<int>(document.Caret().line);
	if(line < scroll)
		scroll = line;
	else if(line >= scroll + VisibleLines())
		scroll = line - VisibleLines() + 1;
}



int CodeEditor::VisibleLines() const
{
	return max(1, static_cast<int>((area.Height() - 2. * PADDING) / LINE_HEIGHT));
}



void CodeEditor::DrawLine(const string &line, const Point &start, bool dim, int lineIndex) const
{
	size_t indent = 0;
	while(indent < line.size() && (line[indent] == '\t' || line[indent] == ' '))
		++indent;
	if(indent == line.size())
		return;

	vector<Color> colors;
	colors.reserve(line.size());
	for(const Token &token : Tokenize(line))
		colors.insert(colors.end(), token.text.size(), TokenColor(token.kind, dim, style));
	colors.erase(colors.begin(), colors.begin() + indent);

	// Leave off whatever runs past the right edge.
	const vector<double> columns = ColumnX(line);
	size_t end = line.size();
	while(end > indent && start.X() + columns[end] > area.Right() - 6.)
		--end;
	string text = Displayed(line.substr(indent, end - indent));
	if(style)
		text = style->Encode(text);
	Point position = start + Point(columns[indent], 0.);
	// While typing, the line being typed on stays readable.
	if(!glitch.empty() && scramble && !(hasFocus && static_cast<size_t>(lineIndex) == document.Caret().line))
	{
		// Some lines jump sideways, and some letters turn into noise, in a
		// pattern that changes every few frames.
		const uint32_t phase = blink / GLITCH_FRAMES;
		const uint32_t jump = Hash(lineIndex, phase, 1);
		if(jump % 4 == 0)
			position += Point(static_cast<int>(jump / 4 % 15) - 7, 0.);
		for(size_t i = 0; i < text.size(); ++i)
		{
			const uint32_t h = Hash(lineIndex, phase, i + 2);
			if(text[i] != ' ' && h % 9 == 0)
			{
				text[i] = NOISE[h / 9 % NOISE.size()];
				colors[i] = GLITCH;
			}
		}
	}
	FontSet::Get(14).DrawCode(text, position, colors);
}



void CodeEditor::DrawGlitchMessage() const
{
	// Bands of interference across the code (unless it should stay readable,
	// in which case a border flickers around it instead).
	const uint32_t phase = blink / GLITCH_FRAMES;
	if(!scramble)
	{
		const float alpha = .55f + .35f * static_cast<float>(Hash(phase, 5, 5) % 3) / 2.f;
		const Color border(GLITCH.Get()[0] * alpha, GLITCH.Get()[1] * alpha, GLITCH.Get()[2] * alpha, alpha);
		LineShader::Draw(area.TopLeft(), Point(area.Right(), area.Top()), 2.f, border, false);
		LineShader::Draw(Point(area.Left(), area.Bottom()), Point(area.Right(), area.Bottom()), 2.f, border, false);
		LineShader::Draw(area.TopLeft(), Point(area.Left(), area.Bottom()), 2.f, border, false);
		LineShader::Draw(Point(area.Right(), area.Top()), Point(area.Right(), area.Bottom()), 2.f, border, false);
	}
	for(int i = 0; scramble && i < 4; ++i)
	{
		const uint32_t h = Hash(phase, i, 99);
		const double y = area.Top() + (h % 1000) / 1000. * area.Height();
		const double height = 1. + h / 1000 % 4;
		FillShader::Fill(Point(area.Center().X(), y), Point(area.Width(), height),
			Color(GLITCH.Get()[0] * .25f, GLITCH.Get()[1] * .25f, GLITCH.Get()[2] * .25f, .25f));
	}

	// The message, in a bar across the bottom.
	const Font &font = FontSet::Get(14);
	const double height = 30.;
	const Point center(area.Center().X(), area.Bottom() - height / 2. - 4.);
	FillShader::Fill(center, Point(area.Width() - 8., height), Color(.12f, .02f, .12f, .95f));
	const double jitter = Hash(phase, 7, 7) % 3 == 0 ? static_cast<double>(Hash(phase, 8, 8) % 5) - 2. : 0.;
	font.Draw(glitch, Point(center.X() - font.Width(glitch) / 2. + jitter, center.Y() - 8.), GLITCH);
}
