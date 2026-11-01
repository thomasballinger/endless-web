/* Script.cpp
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

#include "Script.h"

#include "../DataFile.h"
#include "../DataNode.h"
#include "../Logger.h"
#include "../pi.h"
#include "../Random.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>

using namespace std;

namespace script {

Value::Value() = default;

Value::Value(double number)
	: type(Type::NUMBER), number(number)
{
}

Value::Value(bool boolean)
	: type(Type::BOOLEAN), boolean(boolean)
{
}

Value::Value(const Point &vector)
	: type(Type::VECTOR), vector(vector)
{
}

Value::Value(const RecordList &list)
	: type(Type::LIST), list(&list)
{
}

Value::Value(const RecordList &list, size_t record)
	: type(Type::RECORD), list(&list), record(record)
{
}

Value::Type Value::GetType() const
{
	return type;
}

double Value::Number() const
{
	return number;
}

bool Value::Boolean() const
{
	return boolean;
}

const Point &Value::Vector() const
{
	return vector;
}

const RecordList *Value::List() const
{
	return list;
}

size_t Value::Record() const
{
	return record;
}

const char *Value::TypeName(Type type)
{
	switch(type)
	{
		case Type::NUMBER:
			return "number";
		case Type::BOOLEAN:
			return "boolean";
		case Type::VECTOR:
			return "vector";
		case Type::LIST:
			return "list";
		case Type::RECORD:
			return "record";
	}
	return "unknown";
}

string Value::ToString() const
{
	char buffer[64];
	switch(type)
	{
		case Type::NUMBER:
			snprintf(buffer, sizeof(buffer), "%.4g", number);
			return buffer;
		case Type::BOOLEAN:
			return boolean ? "true" : "false";
		case Type::VECTOR:
			snprintf(buffer, sizeof(buffer), "(%.4g, %.4g)", vector.X(), vector.Y());
			return buffer;
		case Type::LIST:
			return "a list of " + to_string(list ? list->Size() : 0);
		case Type::RECORD:
			return "#" + to_string(record);
	}
	return "?";
}



RecordList::RecordList(int fieldCount)
	: fieldCount(fieldCount)
{
}

void RecordList::Clear()
{
	values.clear();
}

void RecordList::Add()
{
	values.resize(values.size() + fieldCount);
}

void RecordList::SetField(int field, const Value &value)
{
	values[values.size() - fieldCount + field] = value;
}

size_t RecordList::Size() const
{
	return fieldCount ? values.size() / fieldCount : 0;
}

const Value &RecordList::Field(size_t record, int field) const
{
	return values[record * fieldCount + field];
}



int Interface::AddInput(const string &name)
{
	variables.push_back(name);
	outputs.push_back(false);
	listFields.emplace_back();
	return static_cast<int>(variables.size()) - 1;
}

int Interface::AddList(const string &name, const vector<string> &fields)
{
	int variable = AddInput(name);
	listFields.back() = fields;
	return variable;
}

int Interface::AddOutput(const string &name)
{
	variables.push_back(name);
	outputs.push_back(true);
	listFields.emplace_back();
	return static_cast<int>(variables.size()) - 1;
}

int Interface::AddEffect(const string &name)
{
	effects.push_back(name);
	return static_cast<int>(effects.size()) - 1;
}

int Interface::AddFunction(const string &name, int argumentCount, Function function)
{
	functionNames.push_back(name);
	functionArgumentCounts.push_back(argumentCount);
	functions.push_back(std::move(function));
	return static_cast<int>(functions.size()) - 1;
}

int Interface::FindFunction(const string &name) const
{
	for(size_t i = 0; i < functionNames.size(); ++i)
		if(functionNames[i] == name)
			return static_cast<int>(i);
	return -1;
}

int Interface::FunctionArgumentCount(int function) const
{
	return functionArgumentCounts.at(function);
}

const Interface::Function &Interface::GetFunction(int function) const
{
	return functions.at(function);
}

void Interface::SetPrelude(const string &text)
{
	prelude = text;
}



const string &Interface::Prelude() const
{
	return prelude;
}



int Interface::VariableCount() const
{
	return static_cast<int>(variables.size());
}

int Interface::EffectCount() const
{
	return static_cast<int>(effects.size());
}

const string &Interface::VariableName(int variable) const
{
	return variables.at(variable);
}

const string &Interface::EffectName(int effect) const
{
	return effects.at(effect);
}

int Interface::Find(const string &name) const
{
	for(size_t i = 0; i < variables.size(); ++i)
		if(variables[i] == name)
			return static_cast<int>(i);
	auto alias = aliases.find(name);
	return alias == aliases.end() ? -1 : alias->second;
}



void Interface::AddAlias(const string &alias, int variable)
{
	aliases[alias] = variable;
}



const string &Interface::AliasOf(int variable) const
{
	for(const auto &it : aliases)
		if(it.second == variable)
			return it.first;
	return variables[variable];
}



void Interface::SetGuard(int variable, int guard)
{
	if(guards.size() < variables.size())
		guards.resize(variables.size(), -1);
	guards[variable] = guard;
}



int Interface::Guard(int variable) const
{
	return variable < static_cast<int>(guards.size()) ? guards[variable] : -1;
}

bool Interface::IsOutput(int variable) const
{
	return variable >= 0 && variable < static_cast<int>(outputs.size()) && outputs[variable];
}

bool Interface::IsList(int variable) const
{
	return variable >= 0 && variable < static_cast<int>(listFields.size()) && !listFields[variable].empty();
}

int Interface::FindField(int list, const string &field) const
{
	if(!IsList(list))
		return -1;
	const vector<string> &fields = listFields[list];
	for(size_t i = 0; i < fields.size(); ++i)
		if(fields[i] == field)
			return static_cast<int>(i);
	return -1;
}

int Interface::FindEffect(const string &name) const
{
	for(size_t i = 0; i < effects.size(); ++i)
		if(effects[i] == name)
			return static_cast<int>(i);
	return -1;
}



namespace {
	// The most expressions a single run may evaluate. Without loops a program
	// can't exceed its own size, but this keeps the guarantee explicit.
	const int MAX_STEPS = 100000;

	enum class Builtin {
		ADD, SUBTRACT, MULTIPLY, DIVIDE,
		LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, EQUAL, NOT_EQUAL,
		NOT, AND, OR, IF,
		ABS, SQRT, ASIN, ACOS, SIN, COS, ATAN2, MIN, MAX, COPYSIGN, CLAMP, SIGNBIT, IS_NAN,
		VEC, X, Y, DOT, CROSS, UNIT, LENGTH, RANDOM, RANDOM_INT
	};

	class BuiltinInfo {
	public:
		Builtin builtin;
		int minArgs;
		// -1 for any number of arguments.
		int maxArgs;
	};

	const map<string, BuiltinInfo> BUILTINS = {
		{"+", {Builtin::ADD, 2, -1}},
		{"-", {Builtin::SUBTRACT, 1, 2}},
		{"*", {Builtin::MULTIPLY, 2, -1}},
		{"/", {Builtin::DIVIDE, 2, 2}},
		{"<", {Builtin::LESS, 2, 2}},
		{">", {Builtin::GREATER, 2, 2}},
		{"<=", {Builtin::LESS_EQUAL, 2, 2}},
		{">=", {Builtin::GREATER_EQUAL, 2, 2}},
		{"==", {Builtin::EQUAL, 2, 2}},
		{"!=", {Builtin::NOT_EQUAL, 2, 2}},
		{"not", {Builtin::NOT, 1, 1}},
		{"and", {Builtin::AND, 2, -1}},
		{"or", {Builtin::OR, 2, -1}},
		{"if", {Builtin::IF, 3, 3}},
		{"abs", {Builtin::ABS, 1, 1}},
		{"sqrt", {Builtin::SQRT, 1, 1}},
		{"asin", {Builtin::ASIN, 1, 1}},
		{"acos", {Builtin::ACOS, 1, 1}},
		{"sin", {Builtin::SIN, 1, 1}},
		{"cos", {Builtin::COS, 1, 1}},
		{"atan2", {Builtin::ATAN2, 2, 2}},
		{"min", {Builtin::MIN, 2, -1}},
		{"max", {Builtin::MAX, 2, -1}},
		{"copysign", {Builtin::COPYSIGN, 2, 2}},
		{"clamp", {Builtin::CLAMP, 3, 3}},
		{"signbit", {Builtin::SIGNBIT, 1, 1}},
		{"is-nan", {Builtin::IS_NAN, 1, 1}},
		{"vec", {Builtin::VEC, 2, 2}},
		{"x", {Builtin::X, 1, 1}},
		{"y", {Builtin::Y, 1, 1}},
		{"dot", {Builtin::DOT, 2, 2}},
		{"cross", {Builtin::CROSS, 2, 2}},
		{"unit", {Builtin::UNIT, 1, 1}},
		{"length", {Builtin::LENGTH, 1, 1}},
		// Random numbers come from whatever random stream the host has made
		// current, so a script draws them exactly as the code it replaced did.
		{"random", {Builtin::RANDOM, 0, 0}},
		{"random-int", {Builtin::RANDOM_INT, 1, 1}},
	};

	const map<string, double> CONSTANTS = {
		{"pi", PI},
		{"to-deg", TO_DEG},
		{"to-rad", TO_RAD},
	};



	// An s-expression read from one line of a data file.
	class SExpression {
	public:
		bool IsAtom() const { return items.empty() && !isList; }
		// The name as the script wrote it, for showing to a player.
		const string &Name() const { return display.empty() ? atom : display; }

		string atom;
		// In infix scripts, names are written with underscores where the
		// game's names have hyphens ("target_position" for "target-position");
		// the atom is the game's name, and this is how it was written.
		string display;
		bool isList = false;
		vector<SExpression> items;
	};

	// Read the tokens of a data node as a list of s-expressions. Returns an
	// error message, or an empty string if the line was read successfully.
	string ReadLine(const DataNode &node, SExpression &line)
	{
		string text;
		for(int i = 0; i < node.Size(); ++i)
			text += (i ? " " : "") + node.Token(i);

		line = SExpression();
		line.isList = true;
		vector<SExpression *> stack = {&line};
		string token;
		auto flush = [&token, &stack]() {
			if(!token.empty())
			{
				stack.back()->items.emplace_back();
				stack.back()->items.back().atom = token;
				token.clear();
			}
		};
		for(char c : text)
		{
			if(c == '(')
			{
				flush();
				stack.back()->items.emplace_back();
				stack.back()->items.back().isList = true;
				stack.push_back(&stack.back()->items.back());
			}
			else if(c == ')')
			{
				flush();
				if(stack.size() == 1)
					return "Unmatched \")\":";
				stack.pop_back();
			}
			else if(c == ' ' || c == '\t')
				flush();
			else
				token += c;
		}
		flush();
		if(stack.size() != 1)
			return "Unmatched \"(\":";
		if(line.items.empty() || !line.items.front().IsAtom())
			return "A line must begin with a statement keyword:";
		return "";
	}

	// Infix scripts: a C-like way of writing the same programs. Each line is
	// read into the same s-expressions a prefix line would be, so the two
	// compile to exactly the same thing:
	//   let d = target_position - position     (let d (- target-position position))
	//   turn = copysign(turn, desired_turn)     (set turn (copysign turn desired-turn))
	//   if a > b && !c                          (if (and (> a b) (not c)))
	//   else if ...  /  else                    (elif ...) / (else)
	//   x = c ? a : b                           (set x (if c a b))
	//   v.x, v.y                                (x v), (y v)
	class InfixReader {
	public:
		explicit InfixReader(const string &text)
		{
			Tokenize(text);
		}

		// Read a statement, or return an error message.
		string ReadStatement(SExpression &line)
		{
			if(!error.empty())
				return error;
			line = SExpression();
			line.isList = true;
			if(tokens.empty())
				return "A line must begin with a statement keyword:";
			const string first = tokens[0].text;
			auto keyword = [&line](const string &word) {
				line.items.emplace_back();
				line.items.back().atom = word;
			};
			if(first == "let" || first == "memory")
			{
				// "let <name> = <expression>"
				keyword(first);
				Take();
				if(!Name(line) || !Expect("="))
					return Error("Expected \"" + first + " <name> = <expression>\":");
				if(!Expression(line))
					return error;
			}
			else if(first == "if" || first == "elif" || first == "while")
			{
				keyword(first);
				Take();
				if(!Expression(line))
					return error;
			}
			else if(first == "wait")
			{
				// "wait" or "wait <steps>"
				keyword(first);
				Take();
				if(position < tokens.size() && !Expression(line))
					return error;
			}
			else if(first == "else")
			{
				Take();
				if(At("if"))
				{
					Take();
					keyword("elif");
					if(!Expression(line))
						return error;
				}
				else
					keyword("else");
			}
			else if(first == "return")
			{
				keyword(first);
				Take();
				if(!Expression(line))
					return error;
			}
			else if(first == "for")
			{
				// "for <name> in <list>"
				keyword(first);
				Take();
				if(!Name(line) || !At("in"))
					return Error("Expected \"for <name> in <list>\":");
				Take();
				keyword("in");
				if(!Name(line))
					return Error("Expected \"for <name> in <list>\":");
			}
			else if(first == "function" || first == "routine")
			{
				// "function <name>(<parameter>, ...)", or "routine ..."
				const string expected = "Expected \"" + first + " <name>(<parameter>, ...)\":";
				keyword(first);
				Take();
				if(!Name(line) || !Expect("("))
					return Error(expected);
				while(!At(")"))
				{
					if(!Name(line))
						return Error(expected);
					if(!At(")") && !Expect(","))
						return Error(expected);
				}
				Take();
			}
			else if(tokens.size() >= 2 && tokens[1].text == "=" && IsName(tokens[0]))
			{
				// "<name> = <expression>" changes a variable or an output.
				keyword("set");
				if(!Name(line))
					return error;
				Take();
				if(!Expression(line))
					return error;
			}
			else if(tokens.size() >= 3 && IsName(tokens[0]) && tokens[1].text == "(")
			{
				// "<routine>(<argument>, ...)" runs a routine.
				keyword("call");
				if(!Expression(line))
					return error;
			}
			else if(tokens.size() == 1 && IsName(tokens[0]))
			{
				// A statement that is a single word, like "stop" or "loop".
				if(!Name(line))
					return error;
			}
			else
				return Error("Expected a statement, like \"let x = 1\", \"x = 2\" or \"if x > 1\":");
			if(position < tokens.size())
				return Error("Unexpected \"" + tokens[position].text + "\":");
			return "";
		}


	private:
		class Token {
		public:
			string text;
			// A number or a name, rather than an operator.
			bool isWord = false;
		};

		void Tokenize(const string &text)
		{
			static const vector<string> OPERATORS = {"==", "!=", "<=", ">=", "&&", "||", "+", "-", "*", "/", "<", ">",
				"!", "(", ")", ",", "?", ":", "="};
			size_t i = 0;
			while(i < text.size())
			{
				const char c = text[i];
				if(c == ' ' || c == '\t')
				{
					++i;
					continue;
				}
				if(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')
				{
					// A number (with an exponent, maybe signed) or a name, which
					// may have fields ("target.position").
					size_t end = i;
					const bool isNumber = isdigit(static_cast<unsigned char>(c)) || c == '.';
					while(end < text.size() && (isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_'
							|| text[end] == '.' || (isNumber && (text[end] == '+' || text[end] == '-')
								&& (text[end - 1] == 'e' || text[end - 1] == 'E'))))
						++end;
					tokens.push_back({text.substr(i, end - i), true});
					i = end;
					continue;
				}
				bool found = false;
				for(const string &op : OPERATORS)
					if(text.compare(i, op.size(), op) == 0)
					{
						tokens.push_back({op, false});
						i += op.size();
						found = true;
						break;
					}
				if(!found)
				{
					error = string("Unexpected \"") + c + "\":";
					return;
				}
			}
		}

		bool At(const string &text) const
		{
			return position < tokens.size() && tokens[position].text == text;
		}

		void Take()
		{
			++position;
		}

		bool Expect(const string &text)
		{
			if(!At(text))
				return false;
			Take();
			return true;
		}

		string Error(const string &message)
		{
			if(error.empty())
				error = message;
			return error;
		}

		static bool IsName(const Token &token)
		{
			return token.isWord && !isdigit(static_cast<unsigned char>(token.text[0])) && token.text[0] != '.';
		}

		// The game's name for a name written with underscores.
		static SExpression Atom(const string &written)
		{
			SExpression atom;
			atom.atom = written;
			replace(atom.atom.begin(), atom.atom.end(), '_', '-');
			if(atom.atom != written)
				atom.display = written;
			return atom;
		}

		bool Name(SExpression &line)
		{
			if(position >= tokens.size() || !IsName(tokens[position]))
			{
				Error("Expected a name:");
				return false;
			}
			line.items.push_back(Atom(tokens[position].text));
			Take();
			return true;
		}

		bool Expression(SExpression &line)
		{
			line.items.emplace_back();
			return Parse(line.items.back(), 0);
		}

		// How tightly each binary operator binds, and the builtin it is.
		static int Precedence(const string &op)
		{
			static const map<string, int> PRECEDENCE = {{"?", 1}, {"||", 2}, {"or", 2}, {"&&", 3}, {"and", 3},
				{"==", 4}, {"!=", 4}, {"<", 5}, {">", 5}, {"<=", 5}, {">=", 5}, {"+", 6}, {"-", 6}, {"*", 7}, {"/", 7}};
			auto it = PRECEDENCE.find(op);
			return it == PRECEDENCE.end() ? -1 : it->second;
		}

		static SExpression Call(const string &function, vector<SExpression> &&arguments)
		{
			SExpression call;
			call.isList = true;
			call.items.emplace_back();
			call.items.back().atom = function;
			for(SExpression &argument : arguments)
				call.items.push_back(std::move(argument));
			return call;
		}

		bool Parse(SExpression &result, int minimum)
		{
			if(!Primary(result))
				return false;
			while(position < tokens.size())
			{
				const string op = tokens[position].text;
				const int precedence = Precedence(op);
				if(precedence < 0 || precedence <= minimum - (op == "?" ? 1 : 0))
					break;
				Take();
				if(op == "?")
				{
					// "c ? a : b", which groups from the right.
					SExpression yes;
					SExpression no;
					if(!Parse(yes, 0) || !Expect(":") || !Parse(no, precedence - 1))
					{
						Error("Expected \"<condition> ? <value> : <value>\":");
						return false;
					}
					vector<SExpression> arguments;
					arguments.push_back(std::move(result));
					arguments.push_back(std::move(yes));
					arguments.push_back(std::move(no));
					result = Call("if", std::move(arguments));
					continue;
				}
				SExpression right;
				if(!Parse(right, precedence))
					return false;
				const string function = (op == "&&") ? "and" : (op == "||") ? "or" : op;
				vector<SExpression> arguments;
				arguments.push_back(std::move(result));
				arguments.push_back(std::move(right));
				result = Call(function, std::move(arguments));
			}
			return true;
		}

		bool Primary(SExpression &result)
		{
			if(position >= tokens.size())
			{
				Error("An expression is missing:");
				return false;
			}
			const Token token = tokens[position];
			Take();
			if(token.text == "(")
			{
				if(!Parse(result, 0))
					return false;
				if(!Expect(")"))
				{
					Error("Unmatched \"(\":");
					return false;
				}
				return true;
			}
			if(token.text == "-" || token.text == "!" || token.text == "not")
			{
				// Unary operators bind tighter than any binary one.
				SExpression operand;
				if(!Parse(operand, 8))
					return false;
				vector<SExpression> arguments;
				arguments.push_back(std::move(operand));
				result = Call(token.text == "-" ? "-" : "not", std::move(arguments));
				return true;
			}
			if(!token.isWord)
			{
				Error("Unexpected \"" + token.text + "\":");
				return false;
			}
			if(!IsName(token))
			{
				result = SExpression();
				result.atom = token.text;
				return true;
			}
			result = Atom(token.text);
			if(At("("))
			{
				// A call: "name(argument, ...)".
				Take();
				vector<SExpression> arguments;
				while(!At(")"))
				{
					arguments.emplace_back();
					if(!Parse(arguments.back(), 0))
						return false;
					if(!At(")") && !Expect(","))
					{
						Error("Expected \",\" or \")\" in a call to \"" + token.text + "\":");
						return false;
					}
				}
				Take();
				SExpression call = Call(result.atom, std::move(arguments));
				call.items.front().display = result.display;
				result = std::move(call);
			}
			return true;
		}


	private:
		vector<Token> tokens;
		size_t position = 0;
		string error;
	};

	// Whether a line can only be infix: it assigns with "=", uses an operator
	// between two things (in prefix, operators only come right after "("),
	// calls a function as "f(x)", uses an operator only infix has, has a name
	// with an underscore, or says "else if".
	bool IsInfixLine(const DataNode &node)
	{
		static const vector<string> BINARY = {"<", ">", "<=", ">=", "==", "!=", "+", "-", "*", "/"};
		if(node.Size() >= 2 && node.Token(0) == "else" && node.Token(1) == "if")
			return true;
		for(int i = 0; i < node.Size(); ++i)
		{
			const string &token = node.Token(i);
			if(token == "=" || find(BINARY.begin(), BINARY.end(), token) != BINARY.end())
				return true;
			if(token.find("&&") != string::npos || token.find("||") != string::npos || token.find('?') != string::npos
					|| token.find('_') != string::npos || token.find(',') != string::npos || token[0] == '!')
				return true;
			// "f(x)", or an operator inside a token, like "a>1".
			const size_t paren = token.find('(');
			if(paren != string::npos && paren > 0 && token[paren - 1] != '(')
				return true;
			for(size_t j = 1; j < token.size(); ++j)
				if(strchr("<>=*/", token[j]) && isalnum(static_cast<unsigned char>(token[j - 1])))
					return true;
		}
		return false;
	}

	bool IsInfix(const DataNode &node)
	{
		for(const DataNode &child : node)
			if(IsInfixLine(child) || IsInfix(child))
				return true;
		return false;
	}

	bool ParseNumber(const string &text, double &value)
	{
		if(text.empty())
			return false;
		char *end = nullptr;
		value = strtod(text.c_str(), &end);
		return end && *end == '\0';
	}

	string Describe(const DataNode &node)
	{
		string text;
		for(int i = 0; i < node.Size(); ++i)
			text += (i ? " " : "") + node.Token(i);
		return text;
	}
}



namespace {
	// While a program runs on this thread with a trace: the trace, how many
	// function calls deep it is, and the line of the statement running.
	thread_local Trace *currentTrace = nullptr;
	thread_local RandomTape *currentTape = nullptr;
	thread_local Activity *currentActivity = nullptr;
	thread_local int traceDepth = 0;
	thread_local size_t currentLine = 0;

	// While a linear program runs on this thread: where it last waited (the
	// statement at each level of nesting, and its branch), how deep the run
	// is in carrying on from there, how many steps are left to wait, and
	// where it waits next, collected from the innermost level out.
	class LinearRun {
	public:
		const vector<pair<int, int>> *path = nullptr;
		size_t depth = 0;
		bool resuming = false;
		int *waitLeft = nullptr;
		vector<pair<int, int>> next;
		// The line of the script's own code that is running, for statements
		// in the interface's prelude, which have no line of their own.
		size_t visibleLine = 0;
	};
	thread_local LinearRun *currentLinear = nullptr;

	void Record(Trace::Step::Kind kind, size_t line, const string &name, const Value &value)
	{
		currentTrace->steps.push_back({kind, line, name, value, traceDepth});
	}
}



class Program::Implementation {
public:
	class Expression {
	public:
		enum class Kind { CONSTANT, VARIABLE, CALL, HOST, FUNCTION, FIELD };
		Kind kind = Kind::CONSTANT;
		Value constant;
		int slot = -1;
		Builtin builtin = Builtin::ADD;
		// For HOST: which of the interface's functions to call. For FUNCTION:
		// which of the script's own functions. For FIELD: which field of the
		// record in the slot (-1 for the record's index in its list).
		int function = -1;
		vector<Expression> arguments;
	};

	class Block;
	class Statement {
	public:
		enum class Kind { ASSIGN, IF, EFFECT, STOP, MEMORY, RETURN, FOR, WAIT, WHILE, CALL };
		Kind kind = Kind::STOP;
		int slot = -1;
		// For CALL: which routine (with its arguments in "conditions"). For
		// WAIT, "expression" is how many steps; for WHILE, the condition and
		// block are the first of "conditions" and "blocks".
		int routine = -1;
		Expression expression;
		int effect = -1;
		// For MEMORY: which of the program's memory values this is.
		int memory = -1;
		// For FOR: the slot of the list to loop over (the loop variable is in
		// "slot", and the loop's statements in blocks[0]).
		int list = -1;
		// For IF: the condition and block of the "if" and each "elif", then
		// an optional "else" block with no condition.
		vector<Expression> conditions;
		vector<Block> blocks;
		// For statistics.
		string description;
		size_t index = 0;
		// For traces: the line it is on, the name of the variable it sets or
		// effect it triggers, and for IF, the line of each "if" or "elif".
		size_t line = 0;
		string name;
		vector<size_t> branchLines;
	};

	class Block {
	public:
		vector<Statement> statements;
	};

	// A function defined by the script. It can't call itself, even through
	// other functions (a function can only call the ones defined above it),
	// so each of its parameters and variables has a slot of its own.
	class Function {
	public:
		string name;
		vector<int> parameterSlots;
		// Where "return" puts the function's value.
		int returnSlot = -1;
		Block body;
	};

	// A routine: like a function, but run as a statement, and able to set
	// outputs, trigger effects and wait. What it reads, sets and triggers
	// only counts for a program that calls it.
	class Routine {
	public:
		string name;
		vector<int> parameterSlots;
		Block body;
		vector<bool> reads;
		vector<bool> writes;
		vector<bool> triggers;
		bool waits = false;
	};


public:
	bool Compile(const DataNode &node, const Interface &interface);
	bool Run(vector<Value> &variables, uint32_t &effects, Memory *memory) const;


private:
	bool CompileBlock(const DataNode &node, Block &block);
	bool CompileFunction(const DataNode &node, const SExpression &line);
	bool CompileRoutine(const DataNode &node, const SExpression &line);
	// A call of a routine: the routine's name, then its arguments.
	bool CompileCall(const DataNode &node, const SExpression &call, Statement &statement);
	int FindRoutine(const string &name) const;
	bool CompileExpression(const SExpression &source, const DataNode &node, Expression &expression);
	int Lookup(const string &name) const;
	bool IsTaken(const string &name) const;
	// Read a line in whichever way this program is written.
	string ReadStatement(const DataNode &node, SExpression &line) const;
	// Whether the given variable is known to be true here (it is the
	// condition, or part of an "and" condition, of an "if" around this code,
	// or an earlier "if not" stopped otherwise).
	bool IsKnown(int slot) const;
	// The variables an "if" or "elif" condition being true guarantees.
	static void Guarantees(const Expression &condition, vector<int> &slots);

	enum class Result { CONTINUE, STOP, RETURN, ERROR, WAIT };
	Result RunBlock(const Block &block, vector<Value> &variables, uint32_t &effects, Memory *memory,
		int &steps) const;
	bool Evaluate(const Expression &expression, vector<Value> &variables, int &steps, Value &result) const;
	// Carry on running a statement that a linear program was waiting in.
	Result Resume(const Statement &statement, int branch, vector<Value> &variables, uint32_t &effects,
		Memory *memory, int &steps) const;
	// Run a "while" loop, from checking its condition.
	Result RunWhile(const Statement &statement, vector<Value> &variables, uint32_t &effects, Memory *memory,
		int &steps) const;
	// Run a routine (carrying on where it waited, if resuming).
	Result RunCall(const Statement &statement, vector<Value> &variables, uint32_t &effects, Memory *memory,
		int &steps, bool resuming) const;
	bool Fail(const string &message) const;
	void CountStatement(const Statement &statement) const;
	// Report a mistake that keeps the program from compiling.
	void Report(const DataNode &node, const string &message);


public:
	string name;
	bool valid = false;
	int slotCount = 0;
	Block body;
	// Which of the interface's variables and effects the program refers to.
	vector<bool> reads;
	vector<bool> writes;
	vector<bool> triggers;
	// The variable slot of each "memory" value.
	vector<int> memorySlots;
	// The functions the script defines.
	vector<Function> functions;
	// The routines it defines, after those of the interface's prelude.
	vector<Routine> routines;
	// Whether it waits, so that each run picks up where the last one paused.
	bool linear = false;
	// The mistakes found while compiling, and whether to log them.
	vector<Program::Error> errors;
	bool logErrors = true;
	// Different every time a program is compiled, so that memory kept for an
	// older version of it (e.g. before the script was edited and reloaded)
	// isn't used with a different layout.
	uint64_t version = 0;


private:
	const Interface *interface = nullptr;
	// Names visible at each nesting level while compiling.
	vector<map<string, int>> scopes;
	// The function being compiled, if any.
	const Function *compilingFunction = nullptr;
	// The routine being compiled, if any.
	Routine *compilingRoutine = nullptr;
	// How many "for" loops deep the code being compiled is.
	int forDepth = 0;
	// Whether the interface's prelude is being compiled (whose lines aren't
	// in the script's data file, so they get no line numbers).
	bool inPrelude = false;
	// The loop variables in scope, which hold records, and the list input
	// each one loops over.
	map<int, int> recordSlots;
	// Whether the program is written in infix (C-like) form.
	bool infix = false;
	// For each block being compiled, the variables known to be true in it.
	vector<vector<int>> known;
	size_t statementCount = 0;
	mutable bool reportedError = false;
	mutable string error;
};



bool Program::Implementation::Compile(const DataNode &node, const Interface &interface)
{
	static atomic<uint64_t> versions = 0;
	version = ++versions;
	this->interface = &interface;
	slotCount = interface.VariableCount();
	reads.assign(slotCount, false);
	writes.assign(slotCount, false);
	triggers.assign(interface.EffectCount(), false);
	memorySlots.clear();
	errors.clear();
	functions.clear();
	compilingFunction = nullptr;
	recordSlots.clear();
	scopes.assign(1, {});
	known.clear();
	routines.clear();
	compilingRoutine = nullptr;
	forDepth = 0;
	linear = false;
	// The interface's routines come first, as if the script began with them.
	if(!interface.Prelude().empty())
	{
		istringstream in(interface.Prelude());
		DataFile file(in);
		infix = true;
		inPrelude = true;
		for(const DataNode &child : file)
		{
			SExpression line;
			if(!ReadStatement(child, line).empty() || line.items.front().atom != "routine")
				Report(child, "The interface's prelude can only define routines:");
			else
				CompileRoutine(child, line);
		}
		inPrelude = false;
		errors.clear();
	}
	infix = IsInfix(node);
	body = Block();
	valid = CompileBlock(node, body);
	scopes.clear();
	return valid;
}



string Program::Implementation::ReadStatement(const DataNode &node, SExpression &line) const
{
	if(!infix)
		return ReadLine(node, line);
	return InfixReader(Describe(node)).ReadStatement(line);
}



bool Program::Implementation::IsKnown(int slot) const
{
	for(const vector<int> &slots : known)
		if(find(slots.begin(), slots.end(), slot) != slots.end())
			return true;
	return false;
}



void Program::Implementation::Guarantees(const Expression &condition, vector<int> &slots)
{
	if(condition.kind == Expression::Kind::VARIABLE)
		slots.push_back(condition.slot);
	else if(condition.kind == Expression::Kind::CALL && condition.builtin == Builtin::AND)
		for(const Expression &argument : condition.arguments)
			Guarantees(argument, slots);
}



bool Program::Implementation::CompileBlock(const DataNode &node, Block &block)
{
	scopes.emplace_back();
	known.emplace_back();
	bool ok = true;
	for(auto it = node.begin(); it != node.end(); ++it)
	{
		const DataNode &child = *it;
		SExpression line;
		string message = ReadStatement(child, line);
		if(!message.empty())
		{
			Report(child, message);
			ok = false;
			continue;
		}
		const string &keyword = line.items.front().atom;
		const size_t argumentCount = line.items.size() - 1;

		Statement statement;
		statement.description = Describe(child);
		statement.index = statementCount++;
		statement.line = inPrelude ? 0 : child.LineNumber();
		if(keyword == "function" || keyword == "routine")
		{
			if(compilingFunction || compilingRoutine || scopes.size() != 2)
			{
				Report(child, "\"" + keyword + "\" can only be used outside of any block:");
				ok = false;
			}
			else if(keyword == "function")
				ok &= CompileFunction(child, line);
			else
				ok &= CompileRoutine(child, line);
			continue;
		}
		if(keyword == "return")
		{
			if(!compilingFunction)
			{
				Report(child, "\"return\" can only be used in a function:");
				ok = false;
				continue;
			}
			if(argumentCount != 1 || child.HasChildren())
			{
				Report(child, "Expected \"return <expression>\":");
				ok = false;
				continue;
			}
			if(!CompileExpression(line.items[1], child, statement.expression))
			{
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::RETURN;
			statement.slot = compilingFunction->returnSlot;
			block.statements.push_back(std::move(statement));
			continue;
		}
		if(keyword == "for")
		{
			// "for <name> in <list>", with the loop's statements below it.
			if(argumentCount != 3 || !line.items[1].IsAtom() || !line.items[3].IsAtom() || line.items[2].atom != "in"
					|| !child.HasChildren())
			{
				Report(child, "Expected \"for <name> in <list>\" with statements below it:");
				ok = false;
				continue;
			}
			const string &variable = line.items[1].atom;
			const string &listName = line.items[3].atom;
			statement.list = Lookup(listName);
			if(!interface->IsList(statement.list))
			{
				Report(child, "\"" + listName + "\" is not a list:");
				ok = false;
				continue;
			}
			if(IsTaken(variable))
			{
				Report(child, "\"" + variable + "\" is already defined:");
				ok = false;
				continue;
			}
			reads[statement.list] = true;
			statement.kind = Statement::Kind::FOR;
			statement.slot = slotCount++;
			// The loop variable is only visible inside the loop.
			scopes.emplace_back();
			scopes.back()[variable] = statement.slot;
			recordSlots[statement.slot] = statement.list;
			statement.blocks.emplace_back();
			++forDepth;
			ok &= CompileBlock(child, statement.blocks.back());
			--forDepth;
			recordSlots.erase(statement.slot);
			scopes.pop_back();
			block.statements.push_back(std::move(statement));
			continue;
		}
		if(compilingFunction && keyword != "let" && keyword != "set" && keyword != "if"
				&& keyword != "elif" && keyword != "else" && keyword != "while" && keyword != "loop")
		{
			Report(child, "A function can only use \"let\", \"set\", \"if\", \"while\", \"for\" and \"return\""
				" (it can't wait, or run a routine):");
			ok = false;
			continue;
		}
		if(keyword == "wait")
		{
			if(argumentCount > 1 || child.HasChildren())
			{
				Report(child, "Expected \"wait\" or \"wait <steps>\":");
				ok = false;
				continue;
			}
			if(forDepth)
			{
				Report(child, "A \"for\" loop can't wait:");
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::WAIT;
			statement.expression.kind = Expression::Kind::CONSTANT;
			statement.expression.constant = Value(1.);
			if(argumentCount && !CompileExpression(line.items[1], child, statement.expression))
			{
				ok = false;
				continue;
			}
			if(compilingRoutine)
				compilingRoutine->waits = true;
			else
				linear = true;
			block.statements.push_back(std::move(statement));
			continue;
		}
		if(keyword == "while" || keyword == "loop")
		{
			// "while <condition>" or "loop" (forever), with statements below.
			const bool isLoop = (keyword == "loop");
			if(argumentCount != (isLoop ? 0 : 1) || !child.HasChildren())
			{
				Report(child, isLoop ? "Expected \"loop\" alone, with statements below it:"
					: "Expected \"while <condition>\" with statements below it:");
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::WHILE;
			statement.conditions.emplace_back();
			vector<int> guaranteed;
			if(isLoop)
			{
				statement.conditions.back().kind = Expression::Kind::CONSTANT;
				statement.conditions.back().constant = Value(true);
			}
			else if(!CompileExpression(line.items[1], child, statement.conditions.back()))
			{
				ok = false;
				continue;
			}
			else
				Guarantees(statement.conditions.back(), guaranteed);
			statement.branchLines.push_back(statement.line);
			known.push_back(guaranteed);
			statement.blocks.emplace_back();
			ok &= CompileBlock(child, statement.blocks.back());
			known.pop_back();
			block.statements.push_back(std::move(statement));
			continue;
		}
		if(keyword == "call" || (FindRoutine(keyword) >= 0 && line.items.front().IsAtom()))
		{
			// In infix, "call" and then the routine and its arguments, as a
			// list; in prefix, the routine's name and its arguments.
			const SExpression &call = (keyword == "call" && argumentCount == 1) ? line.items[1] : line;
			if(child.HasChildren() || (keyword == "call" && (argumentCount != 1 || !line.items[1].isList)))
			{
				Report(child, "Expected \"<routine>(<argument>, ...)\":");
				ok = false;
				continue;
			}
			if(!CompileCall(child, call, statement))
			{
				ok = false;
				continue;
			}
			block.statements.push_back(std::move(statement));
			continue;
		}
		if(keyword == "let" || keyword == "set" || keyword == "memory")
		{
			if(argumentCount != 2 || !line.items[1].IsAtom() || child.HasChildren())
			{
				Report(child, "Expected \"" + keyword + " <name> <expression>\":");
				ok = false;
				continue;
			}
			const string &variable = line.items[1].atom;
			statement.name = line.items[1].Name();
			if(!CompileExpression(line.items[2], child, statement.expression))
			{
				// Still define the variable, so that this one mistake isn't
				// reported again everywhere the variable is used.
				if(keyword != "set" && Lookup(variable) < 0 && !CONSTANTS.contains(variable))
					scopes.back()[variable] = slotCount++;
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::ASSIGN;
			if(keyword == "memory")
			{
				// Memory is kept for the whole program, so it can only be
				// declared in the top-level block.
				if(scopes.size() != 2)
				{
					Report(child, "\"memory\" can only be used outside of any block:");
					ok = false;
					continue;
				}
				if(IsTaken(variable))
				{
					Report(child, "\"" + variable + "\" is already defined:");
					ok = false;
					continue;
				}
				statement.kind = Statement::Kind::MEMORY;
				statement.slot = slotCount++;
				statement.memory = static_cast<int>(memorySlots.size());
				memorySlots.push_back(statement.slot);
				scopes.back()[variable] = statement.slot;
			}
			else if(keyword == "let")
			{
				if(IsTaken(variable))
				{
					Report(child, "\"" + variable + "\" is already defined:");
					ok = false;
					continue;
				}
				statement.slot = slotCount++;
				scopes.back()[variable] = statement.slot;
			}
			else
			{
				statement.slot = Lookup(variable);
				if(statement.slot < 0)
				{
					Report(child, "Unknown variable \"" + statement.name + "\":");
					ok = false;
					continue;
				}
				if(statement.slot < interface->VariableCount() && !interface->IsOutput(statement.slot))
				{
					Report(child, "\"" + statement.name + "\" is an input and can't be changed:");
					ok = false;
					continue;
				}
				if(statement.slot < interface->VariableCount() && compilingFunction)
				{
					Report(child, "A function can't change \"" + variable + "\"; it can only return a value:");
					ok = false;
					continue;
				}
				if(statement.slot < interface->VariableCount())
					writes[statement.slot] = true;
			}
		}
		else if(keyword == "if")
		{
			statement.kind = Statement::Kind::IF;
			// Gather this "if" and any "elif" or "else" lines that follow it.
			auto branch = it;
			while(true)
			{
				SExpression branchLine;
				ReadStatement(*branch, branchLine);
				const string &branchKeyword = branchLine.items.front().atom;
				bool isElse = (branchKeyword == "else");
				// In an "if" or "elif" block, what its condition says holds.
				vector<int> guaranteed;
				if(isElse ? branchLine.items.size() != 1 : branchLine.items.size() != 2)
				{
					Report(*branch, isElse ? "Expected \"else\" alone on its line:"
						: "Expected \"" + branchKeyword + " <condition>\":");
					ok = false;
				}
				else if(!isElse)
				{
					statement.branchLines.push_back(branch->LineNumber());
					statement.conditions.emplace_back();
					ok &= CompileExpression(branchLine.items[1], *branch, statement.conditions.back());
					Guarantees(statement.conditions.back(), guaranteed);
				}
				known.push_back(guaranteed);
				statement.blocks.emplace_back();
				ok &= CompileBlock(*branch, statement.blocks.back());
				known.pop_back();

				auto next = branch;
				++next;
				if(isElse || next == node.end())
					break;
				SExpression nextLine;
				if(!ReadStatement(*next, nextLine).empty())
					break;
				const string &nextKeyword = nextLine.items.front().atom;
				if(nextKeyword != "elif" && nextKeyword != "else")
					break;
				branch = next;
			}
			it = branch;
			// After "if not x" whose block always stops (with no other branch),
			// x is known to be true for the rest of this block.
			if(statement.conditions.size() == 1 && statement.blocks.size() == 1)
			{
				const Expression &condition = statement.conditions.front();
				const vector<Statement> &inside = statement.blocks.front().statements;
				if(condition.kind == Expression::Kind::CALL && condition.builtin == Builtin::NOT && !inside.empty()
						&& (inside.back().kind == Statement::Kind::STOP || inside.back().kind == Statement::Kind::RETURN))
					Guarantees(condition.arguments.front(), known.back());
			}
		}
		else if(keyword == "elif" || keyword == "else")
		{
			Report(child, "\"" + keyword + "\" must follow an \"if\" or \"elif\":");
			ok = false;
			continue;
		}
		else if(keyword == "stop")
		{
			if(argumentCount || child.HasChildren())
			{
				Report(child, "Expected \"stop\" alone on its line:");
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::STOP;
		}
		else
		{
			statement.effect = interface->FindEffect(keyword);
			if(statement.effect < 0)
			{
				Report(child, "Unknown statement \"" + line.items.front().Name() + "\":");
				ok = false;
				continue;
			}
			if(argumentCount || child.HasChildren())
			{
				Report(child, "\"" + keyword + "\" takes no arguments:");
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::EFFECT;
			statement.name = line.items.front().Name();
			triggers[statement.effect] = true;
		}
		block.statements.push_back(std::move(statement));
	}
	scopes.pop_back();
	known.pop_back();
	return ok;
}



// "function <name> <parameter>...", with the function's statements below it.
bool Program::Implementation::CompileFunction(const DataNode &node, const SExpression &line)
{
	if(line.items.size() < 2 || !node.HasChildren())
	{
		Report(node, "Expected \"function <name> <parameter>...\" with statements below it:");
		return false;
	}
	Function function;
	function.name = line.items[1].atom;
	if(!line.items[1].IsAtom() || BUILTINS.contains(function.name) || interface->FindFunction(function.name) >= 0
			|| any_of(functions.begin(), functions.end(),
				[&function](const Function &other) { return other.name == function.name; }))
	{
		Report(node, "A function named \"" + function.name + "\" already exists:");
		return false;
	}

	// The function sees only its parameters and its own variables, along with
	// the inputs and outputs, constants, and functions defined above it.
	vector<map<string, int>> outerScopes = std::move(scopes);
	vector<vector<int>> outerKnown = std::move(known);
	scopes.assign(1, {});
	known.clear();
	bool ok = true;
	for(size_t i = 2; i < line.items.size(); ++i)
	{
		const string &parameter = line.items[i].atom;
		if(!line.items[i].IsAtom() || IsTaken(parameter))
		{
			Report(node, "\"" + parameter + "\" can't be the name of a parameter:");
			ok = false;
			continue;
		}
		function.parameterSlots.push_back(slotCount);
		scopes.back()[parameter] = slotCount++;
	}
	function.returnSlot = slotCount++;
	compilingFunction = &function;
	ok &= CompileBlock(node, function.body);
	compilingFunction = nullptr;
	scopes = std::move(outerScopes);
	known = std::move(outerKnown);
	// Only now can later code call the function, so it can't call itself.
	functions.push_back(std::move(function));
	return ok;
}



// "routine <name> <parameter>...", with the routine's statements below it.
bool Program::Implementation::CompileRoutine(const DataNode &node, const SExpression &line)
{
	if(line.items.size() < 2 || !node.HasChildren())
	{
		Report(node, "Expected \"routine <name> <parameter>...\" with statements below it:");
		return false;
	}
	Routine routine;
	routine.name = line.items[1].atom;
	if(!line.items[1].IsAtom() || BUILTINS.contains(routine.name) || interface->FindFunction(routine.name) >= 0
			|| FindRoutine(routine.name) >= 0 || interface->FindEffect(routine.name) >= 0
			|| any_of(functions.begin(), functions.end(),
				[&routine](const Function &other) { return other.name == routine.name; }))
	{
		Report(node, "A function or routine named \"" + routine.name + "\" already exists:");
		return false;
	}

	// Like a function, a routine sees only its parameters and its own
	// variables, along with the inputs and outputs, constants, and the
	// functions and routines defined above it. What it reads and sets is
	// noted separately, for the programs that call it.
	vector<map<string, int>> outerScopes = std::move(scopes);
	vector<vector<int>> outerKnown = std::move(known);
	vector<bool> outerReads = std::move(reads);
	vector<bool> outerWrites = std::move(writes);
	vector<bool> outerTriggers = std::move(triggers);
	reads.assign(interface->VariableCount(), false);
	writes.assign(interface->VariableCount(), false);
	triggers.assign(interface->EffectCount(), false);
	scopes.assign(1, {});
	known.clear();
	bool ok = true;
	for(size_t i = 2; i < line.items.size(); ++i)
	{
		const string &parameter = line.items[i].atom;
		if(!line.items[i].IsAtom() || IsTaken(parameter))
		{
			Report(node, "\"" + parameter + "\" can't be the name of a parameter:");
			ok = false;
			continue;
		}
		routine.parameterSlots.push_back(slotCount);
		scopes.back()[parameter] = slotCount++;
	}
	compilingRoutine = &routine;
	ok &= CompileBlock(node, routine.body);
	compilingRoutine = nullptr;
	routine.reads = std::move(reads);
	routine.writes = std::move(writes);
	routine.triggers = std::move(triggers);
	reads = std::move(outerReads);
	writes = std::move(outerWrites);
	triggers = std::move(outerTriggers);
	scopes = std::move(outerScopes);
	known = std::move(outerKnown);
	// Only now can later code call the routine, so it can't call itself.
	routines.push_back(std::move(routine));
	return ok;
}



bool Program::Implementation::CompileCall(const DataNode &node, const SExpression &call, Statement &statement)
{
	if(call.items.empty() || !call.items.front().IsAtom())
	{
		Report(node, "Expected \"<routine>(<argument>, ...)\":");
		return false;
	}
	const string &name = call.items.front().atom;
	statement.routine = FindRoutine(name);
	if(statement.routine < 0)
	{
		Report(node, "Unknown routine \"" + call.items.front().Name() + "\":");
		return false;
	}
	const Routine &routine = routines[statement.routine];
	if(call.items.size() - 1 != routine.parameterSlots.size())
	{
		Report(node, "\"" + call.items.front().Name() + "\" takes " + to_string(routine.parameterSlots.size())
			+ (routine.parameterSlots.size() == 1 ? " argument:" : " arguments:"));
		return false;
	}
	if(routine.waits && forDepth)
	{
		Report(node, "A \"for\" loop can't wait, so it can't run \"" + call.items.front().Name() + "\":");
		return false;
	}
	bool ok = true;
	for(size_t i = 1; i < call.items.size(); ++i)
	{
		statement.conditions.emplace_back();
		ok &= CompileExpression(call.items[i], node, statement.conditions.back());
	}
	statement.kind = Statement::Kind::CALL;
	statement.name = call.items.front().Name();
	// The program now does whatever the routine does.
	for(size_t i = 0; i < routine.reads.size(); ++i)
	{
		if(routine.reads[i])
			reads[i] = true;
		if(routine.writes[i])
			writes[i] = true;
	}
	for(size_t i = 0; i < routine.triggers.size(); ++i)
		if(routine.triggers[i])
			triggers[i] = true;
	if(routine.waits)
	{
		if(compilingRoutine)
			compilingRoutine->waits = true;
		else
			linear = true;
	}
	return ok;
}



int Program::Implementation::FindRoutine(const string &name) const
{
	for(size_t i = 0; i < routines.size(); ++i)
		if(routines[i].name == name)
			return static_cast<int>(i);
	return -1;
}



bool Program::Implementation::CompileExpression(const SExpression &source, const DataNode &node,
	Expression &expression)
{
	if(source.IsAtom())
	{
		double number = 0.;
		if(ParseNumber(source.atom, number))
		{
			expression.kind = Expression::Kind::CONSTANT;
			expression.constant = Value(number);
			return true;
		}
		if(source.atom == "true" || source.atom == "false")
		{
			expression.kind = Expression::Kind::CONSTANT;
			expression.constant = Value(source.atom == "true");
			return true;
		}
		auto constant = CONSTANTS.find(source.atom);
		if(constant != CONSTANTS.end())
		{
			expression.kind = Expression::Kind::CONSTANT;
			expression.constant = Value(constant->second);
			return true;
		}
		// A loop variable's fields: "name.field".
		size_t dot = infix ? source.atom.rfind('.') : source.atom.find('.');
		if(dot != string::npos)
		{
			const string base = source.atom.substr(0, dot);
			const string field = source.atom.substr(dot + 1);
			expression.slot = (base.find('.') == string::npos) ? Lookup(base) : -1;
			auto record = recordSlots.find(expression.slot);
			if(record == recordSlots.end())
			{
				// In infix, "target.position" is "target-position", and "v.x" and
				// "v.y" are a vector's coordinates (so "target.position.x" is
				// the x coordinate of "target-position").
				if(infix)
				{
					const string written = source.Name();
					SExpression named;
					named.atom = base + "-" + field;
					named.display = written;
					if(base.find('.') == string::npos && Lookup(named.atom) >= 0)
						return CompileExpression(named, node, expression);
					if(field == "x" || field == "y")
					{
						expression.kind = Expression::Kind::CALL;
						expression.builtin = (field == "x") ? Builtin::X : Builtin::Y;
						expression.arguments.resize(1);
						SExpression vector;
						vector.atom = base;
						vector.display = written.substr(0, written.rfind('.'));
						return CompileExpression(vector, node, expression.arguments.front());
					}
				}
				if(infix)
					Report(node, "Unknown name \"" + source.Name() + "\":");
				else
					Report(node, "\"" + source.Name().substr(0, source.Name().find('.'))
						+ "\" has no fields; only a \"for\" loop's variable does:");
				return false;
			}
			expression.kind = Expression::Kind::FIELD;
			expression.function = (field == "index") ? -1 : interface->FindField(record->second, field);
			if(field != "index" && expression.function < 0)
			{
				Report(node, "\"" + interface->VariableName(record->second) + "\" has no field \"" + field + "\":");
				return false;
			}
			return true;
		}
		expression.kind = Expression::Kind::VARIABLE;
		expression.slot = Lookup(source.atom);
		if(expression.slot < 0)
		{
			Report(node, "Unknown variable \"" + source.Name() + "\":");
			return false;
		}
		// Some inputs only mean something while another is true, like what a
		// seeker sees of its target while it sees it. Infix scripts may only
		// read them where that is certain.
		const int guard = (expression.slot < interface->VariableCount()) ? interface->Guard(expression.slot) : -1;
		if(infix && guard >= 0 && (compilingFunction || !IsKnown(guard)))
		{
			const string guardName = interface->AliasOf(guard);
			Report(node, "\"" + source.Name() + "\" is only known while \"" + guardName + "\" is true: use it inside \"if "
				+ guardName + "\"" + (compilingFunction ? " and pass it to the function" : "") + ":");
			return false;
		}
		// Lists and records can't be copied or compared, only looped over
		// and read, so they can never outlive the run they were given to.
		if(interface->IsList(expression.slot))
		{
			Report(node, "\"" + source.atom + "\" is a list; loop over it with \"for\":");
			return false;
		}
		if(recordSlots.contains(expression.slot))
		{
			Report(node, "\"" + source.atom + "\" is a record; use its fields, like \"" + source.atom
				+ ".index\":");
			return false;
		}
		if(expression.slot < interface->VariableCount())
			reads[expression.slot] = true;
		return true;
	}

	if(source.items.empty() || !source.items.front().IsAtom())
	{
		Report(node, "An expression in parentheses must begin with a function name:");
		return false;
	}
	const string &function = source.items.front().atom;
	const string &functionName = source.items.front().Name();
	int count = static_cast<int>(source.items.size()) - 1;
	auto it = BUILTINS.find(function);
	// The script may define functions of its own.
	auto defined = find_if(functions.begin(), functions.end(),
		[&function](const Function &candidate) { return candidate.name == function; });
	if(defined != functions.end())
	{
		if(count != static_cast<int>(defined->parameterSlots.size()))
		{
			Report(node, "Wrong number of arguments to \"" + functionName + "\":");
			return false;
		}
		expression.kind = Expression::Kind::FUNCTION;
		expression.function = static_cast<int>(defined - functions.begin());
		expression.arguments.resize(count);
		bool ok = true;
		for(int i = 0; i < count; ++i)
			ok &= CompileExpression(source.items[i + 1], node, expression.arguments[i]);
		return ok;
	}
	if(it == BUILTINS.end())
	{
		// The host may provide functions of its own.
		int host = interface->FindFunction(function);
		if(host < 0)
		{
			Report(node, "Unknown function \"" + functionName + "\":");
			return false;
		}
		if(count != interface->FunctionArgumentCount(host))
		{
			Report(node, "Wrong number of arguments to \"" + functionName + "\":");
			return false;
		}
		expression.kind = Expression::Kind::HOST;
		expression.function = host;
		expression.arguments.resize(count);
		bool ok = true;
		for(int i = 0; i < count; ++i)
			ok &= CompileExpression(source.items[i + 1], node, expression.arguments[i]);
		return ok;
	}
	if(count < it->second.minArgs || (it->second.maxArgs >= 0 && count > it->second.maxArgs))
	{
		Report(node, "Wrong number of arguments to \"" + functionName + "\":");
		return false;
	}
	expression.kind = Expression::Kind::CALL;
	expression.builtin = it->second.builtin;
	expression.arguments.resize(count);
	bool ok = true;
	// "and" only evaluates each argument if the ones before it were true, and
	// "c ? a : b" only evaluates a if c is, so what those say holds there
	// (as in "target && target.distance < 500").
	const bool isAnd = (expression.builtin == Builtin::AND);
	const bool isChoice = (expression.builtin == Builtin::IF);
	known.emplace_back();
	for(int i = 0; i < count; ++i)
	{
		ok &= CompileExpression(source.items[i + 1], node, expression.arguments[i]);
		if(isAnd || (isChoice && i == 0))
			Guarantees(expression.arguments[i], known.back());
		else if(isChoice && i == 1)
			known.back().clear();
	}
	known.pop_back();
	return ok;
}



// Whether a "let" or "memory" can't define a variable with this name. A
// script's own variable may hide one of the host's inputs, so that adding an
// input can't break a script that already uses the name, but it may not hide
// an output, a constant, or another of the script's variables.
bool Program::Implementation::IsTaken(const string &name) const
{
	for(const auto &scope : scopes)
		if(scope.contains(name))
			return true;
	const int slot = interface->Find(name);
	return CONSTANTS.contains(name) || (slot >= 0 && interface->IsOutput(slot));
}



int Program::Implementation::Lookup(const string &name) const
{
	for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
	{
		auto it = scope->find(name);
		if(it != scope->end())
			return it->second;
	}
	return interface->Find(name);
}



namespace {
	// When ES_SCRIPT_STATS is set, count how many times each statement runs
	// and print the counts when the game exits.
	map<string, uint64_t> statementStats;

	void PrintStatementStats()
	{
		printf("Script statement counts:\n");
		for(const auto &[statement, count] : statementStats)
			printf("  %10llu  %s\n", static_cast<unsigned long long>(count), statement.c_str());
		fflush(stdout);
	}
}



void Program::Implementation::CountStatement(const Statement &statement) const
{
	static const bool collect = getenv("ES_SCRIPT_STATS");
	if(!collect)
		return;
	if(statementStats.empty())
		atexit(PrintStatementStats);
	char prefix[16];
	snprintf(prefix, sizeof(prefix), " %03zu ", statement.index);
	++statementStats[name + prefix + statement.description];
}



bool Program::Implementation::Run(vector<Value> &variables, uint32_t &effects, Memory *memory) const
{
	if(!valid)
		return false;
	if(static_cast<int>(variables.size()) < slotCount)
		variables.resize(slotCount);
	// Restore the values remembered from the last run.
	if(memory)
	{
		if(memory->version != version)
		{
			memory->Clear();
			memory->version = version;
		}
		memory->values.resize(memorySlots.size());
		memory->isSet.resize(memorySlots.size(), false);
		if(!linear)
			for(size_t i = 0; i < memorySlots.size(); ++i)
				if(memory->isSet[i])
					variables[memorySlots[i]] = memory->values[i];
	}
	int steps = 0;
	if(!linear || !memory)
	{
		Result result = RunBlock(body, variables, effects, memory, steps);
		if(result == Result::ERROR)
			return false;
		if(memory)
			for(size_t i = 0; i < memorySlots.size(); ++i)
				if(memory->isSet[i])
					memory->values[i] = variables[memorySlots[i]];
		return true;
	}

	// A linear program keeps every variable and output it had when it last
	// waited, and carries on from there (or, once it has finished, just
	// keeps its outputs as they were).
	const int inputs = interface->VariableCount();
	if(memory->started)
		for(int slot = 0; slot < slotCount && slot < static_cast<int>(memory->slots.size()); ++slot)
			if(slot >= inputs || interface->IsOutput(slot))
				variables[slot] = memory->slots[slot];
	if(memory->finished)
		return true;
	LinearRun run;
	run.path = &memory->resume;
	run.resuming = memory->started && !memory->resume.empty();
	run.waitLeft = &memory->waitLeft;
	LinearRun *previous = currentLinear;
	currentLinear = &run;
	Result result = RunBlock(body, variables, effects, memory, steps);
	currentLinear = previous;
	if(result == Result::ERROR)
		return false;
	memory->started = true;
	if(result == Result::WAIT)
		memory->resume.assign(run.next.rbegin(), run.next.rend());
	else
	{
		memory->finished = true;
		memory->resume.clear();
	}
	memory->slots.assign(variables.begin(), variables.begin() + slotCount);
	return true;
}



Program::Implementation::Result Program::Implementation::RunBlock(const Block &block, vector<Value> &variables,
	uint32_t &effects, Memory *memory, int &steps) const
{
	LinearRun *run = currentLinear;
	size_t first = 0;
	if(run && run->resuming)
		first = static_cast<size_t>((*run->path)[run->depth].first);
	for(size_t index = first; index < block.statements.size(); ++index)
	{
		const Statement &statement = block.statements[index];
		CountStatement(statement);
		currentLine = statement.line;
		// Carry on from where a linear program waited.
		if(run && run->resuming)
		{
			const int branch = (*run->path)[run->depth].second;
			Result result = Resume(statement, branch, variables, effects, memory, steps);
			if(result == Result::WAIT)
				run->next.emplace_back(static_cast<int>(index), branch);
			if(result != Result::CONTINUE)
				return result;
			continue;
		}
		switch(statement.kind)
		{
			case Statement::Kind::ASSIGN:
			{
				Value value;
				if(!Evaluate(statement.expression, variables, steps, value))
					return Result::ERROR;
				variables[statement.slot] = value;
				if(currentTrace)
					Record(Trace::Step::Kind::ASSIGN, statement.line, statement.name, value);
				break;
			}
			case Statement::Kind::IF:
			{
				size_t chosen = statement.blocks.size();
				for(size_t i = 0; i < statement.conditions.size(); ++i)
				{
					Value condition;
					if(!Evaluate(statement.conditions[i], variables, steps, condition))
						return Result::ERROR;
					if(condition.GetType() != Value::Type::BOOLEAN)
					{
						Fail("a condition must be true or false, not a "
							+ string(Value::TypeName(condition.GetType())));
						return Result::ERROR;
					}
					if(currentTrace)
						Record(Trace::Step::Kind::CONDITION, statement.branchLines[i], "", condition);
					if(condition.Boolean())
					{
						chosen = i;
						break;
					}
				}
				// With no condition true, run the "else" block, if there is one.
				if(chosen == statement.blocks.size() && statement.blocks.size() > statement.conditions.size())
					chosen = statement.blocks.size() - 1;
				if(chosen < statement.blocks.size())
				{
					Result result = RunBlock(statement.blocks[chosen], variables, effects, memory, steps);
					if(result == Result::WAIT && run)
						run->next.emplace_back(static_cast<int>(index), static_cast<int>(chosen));
					if(result != Result::CONTINUE)
						return result;
				}
				break;
			}
			case Statement::Kind::EFFECT:
				effects |= (1u << statement.effect);
				if(currentTrace)
					Record(Trace::Step::Kind::EFFECT, statement.line, statement.name, Value());
				break;
			case Statement::Kind::STOP:
				if(currentTrace)
					Record(Trace::Step::Kind::STOP, statement.line, "", Value());
				return Result::STOP;
			case Statement::Kind::RETURN:
				if(!Evaluate(statement.expression, variables, steps, variables[statement.slot]))
					return Result::ERROR;
				if(currentTrace)
					Record(Trace::Step::Kind::RETURN, statement.line, "", variables[statement.slot]);
				return Result::RETURN;
			case Statement::Kind::FOR:
			{
				const Value &listValue = variables[statement.list];
				if(listValue.GetType() != Value::Type::LIST || !listValue.List())
				{
					Fail("the host didn't give the script the list it loops over");
					return Result::ERROR;
				}
				const RecordList &list = *listValue.List();
				for(size_t i = 0; i < list.Size(); ++i)
				{
					if(++steps > MAX_STEPS)
					{
						Fail("ran for too many steps");
						return Result::ERROR;
					}
					variables[statement.slot] = Value(list, i);
					if(currentTrace)
						Record(Trace::Step::Kind::LOOP, statement.line, "", Value(static_cast<double>(i)));
					Result result = RunBlock(statement.blocks.front(), variables, effects, memory, steps);
					if(result != Result::CONTINUE)
						return result;
				}
				break;
			}
			case Statement::Kind::WAIT:
			{
				Value count;
				if(!Evaluate(statement.expression, variables, steps, count))
					return Result::ERROR;
				if(count.GetType() != Value::Type::NUMBER)
				{
					Fail("\"wait\" takes a number of steps, not a " + string(Value::TypeName(count.GetType())));
					return Result::ERROR;
				}
				const int wait = max(1, static_cast<int>(round(min(1e6, count.Number()))));
				if(currentTrace)
					Record(Trace::Step::Kind::WAIT, statement.line ? statement.line : (run ? run->visibleLine : 0), "",
						Value(static_cast<double>(wait)));
				if(run)
				{
					*run->waitLeft = wait;
					run->next.emplace_back(static_cast<int>(index), 0);
				}
				return Result::WAIT;
			}
			case Statement::Kind::WHILE:
			case Statement::Kind::CALL:
			{
				Result result = (statement.kind == Statement::Kind::WHILE)
					? RunWhile(statement, variables, effects, memory, steps)
					: RunCall(statement, variables, effects, memory, steps, false);
				if(result == Result::WAIT && run)
					run->next.emplace_back(static_cast<int>(index), 0);
				if(result != Result::CONTINUE)
					return result;
				break;
			}
			case Statement::Kind::MEMORY:
			{
				// The initial value is only used on the first run.
				if(!memory || !memory->isSet[statement.memory])
				{
					Value value;
					if(!Evaluate(statement.expression, variables, steps, value))
						return Result::ERROR;
					variables[statement.slot] = value;
					if(memory)
						memory->isSet[statement.memory] = true;
				}
				if(currentTrace)
					Record(Trace::Step::Kind::ASSIGN, statement.line, statement.name, variables[statement.slot]);
				break;
			}
		}
	}
	return Result::CONTINUE;
}



Program::Implementation::Result Program::Implementation::Resume(const Statement &statement, int branch,
	vector<Value> &variables, uint32_t &effects, Memory *memory, int &steps) const
{
	LinearRun &run = *currentLinear;
	switch(statement.kind)
	{
		case Statement::Kind::WAIT:
			// This is where the program waited: wait some more, or move on.
			run.resuming = false;
			if(--*run.waitLeft > 0)
			{
				if(currentTrace)
					Record(Trace::Step::Kind::WAIT, statement.line ? statement.line : run.visibleLine, "",
						Value(static_cast<double>(*run.waitLeft)));
				return Result::WAIT;
			}
			return Result::CONTINUE;
		case Statement::Kind::IF:
		{
			if(branch < 0 || branch >= static_cast<int>(statement.blocks.size()))
			{
				Fail("lost its place in the script");
				return Result::ERROR;
			}
			++run.depth;
			Result result = RunBlock(statement.blocks[branch], variables, effects, memory, steps);
			--run.depth;
			return result;
		}
		case Statement::Kind::WHILE:
		{
			// Finish the pass through the loop it was waiting in, then loop.
			++run.depth;
			Result result = RunBlock(statement.blocks.front(), variables, effects, memory, steps);
			--run.depth;
			if(result != Result::CONTINUE)
				return result;
			return RunWhile(statement, variables, effects, memory, steps);
		}
		case Statement::Kind::CALL:
			return RunCall(statement, variables, effects, memory, steps, true);
		default:
			// The program changed since it waited here (which shouldn't happen,
			// since its memory is cleared when it is recompiled).
			Fail("lost its place in the script");
			return Result::ERROR;
	}
}



Program::Implementation::Result Program::Implementation::RunWhile(const Statement &statement,
	vector<Value> &variables, uint32_t &effects, Memory *memory, int &steps) const
{
	while(true)
	{
		if(++steps > MAX_STEPS)
		{
			Fail("looped too many times without waiting");
			return Result::ERROR;
		}
		Value condition;
		if(!Evaluate(statement.conditions.front(), variables, steps, condition))
			return Result::ERROR;
		if(condition.GetType() != Value::Type::BOOLEAN)
		{
			Fail("a condition must be true or false, not a " + string(Value::TypeName(condition.GetType())));
			return Result::ERROR;
		}
		if(currentTrace)
			Record(Trace::Step::Kind::CONDITION, statement.line, "", condition);
		if(!condition.Boolean())
			return Result::CONTINUE;
		Result result = RunBlock(statement.blocks.front(), variables, effects, memory, steps);
		if(result != Result::CONTINUE)
			return result;
	}
}



Program::Implementation::Result Program::Implementation::RunCall(const Statement &statement,
	vector<Value> &variables, uint32_t &effects, Memory *memory, int &steps, bool resuming) const
{
	const Routine &routine = routines[statement.routine];
	LinearRun *run = currentLinear;
	if(!resuming)
	{
		// Evaluate every argument before setting any parameter.
		vector<Value> values(statement.conditions.size());
		for(size_t i = 0; i < values.size(); ++i)
			if(!Evaluate(statement.conditions[i], variables, steps, values[i]))
				return Result::ERROR;
		for(size_t i = 0; i < values.size(); ++i)
			variables[routine.parameterSlots[i]] = values[i];
	}
	if(currentTrace && statement.line)
		Record(Trace::Step::Kind::CALL, statement.line, statement.name, Value());
	// Statements of the prelude's routines show on the line that called them.
	size_t outerLine = 0;
	if(run)
	{
		outerLine = run->visibleLine;
		if(statement.line)
			run->visibleLine = statement.line;
		if(resuming)
			++run->depth;
	}
	++traceDepth;
	Result result = RunBlock(routine.body, variables, effects, memory, steps);
	--traceDepth;
	if(run)
	{
		if(resuming)
			--run->depth;
		run->visibleLine = outerLine;
	}
	return result;
}



bool Program::Implementation::Evaluate(const Expression &expression, vector<Value> &variables, int &steps,
	Value &result) const
{
	if(++steps > MAX_STEPS)
		return Fail("ran for too many steps");

	switch(expression.kind)
	{
		case Expression::Kind::CONSTANT:
			result = expression.constant;
			return true;
		case Expression::Kind::VARIABLE:
			result = variables[expression.slot];
			return true;
		case Expression::Kind::CALL:
			break;
		case Expression::Kind::FIELD:
		{
			const Value &record = variables[expression.slot];
			if(expression.function < 0)
				result = Value(static_cast<double>(record.Record()));
			else
				result = record.List()->Field(record.Record(), expression.function);
			return true;
		}
		case Expression::Kind::FUNCTION:
		{
			const Function &function = functions[expression.function];
			// Evaluate every argument before setting any parameter, in case an
			// argument calls the same function.
			const size_t count = expression.arguments.size();
			Value small[4];
			vector<Value> large(count > 4 ? count : 0);
			Value *values = count > 4 ? large.data() : small;
			for(size_t i = 0; i < count; ++i)
				if(!Evaluate(expression.arguments[i], variables, steps, values[i]))
					return false;
			for(size_t i = 0; i < count; ++i)
				variables[function.parameterSlots[i]] = values[i];
			uint32_t noEffects = 0;
			++traceDepth;
			Result ran = RunBlock(function.body, variables, noEffects, nullptr, steps);
			--traceDepth;
			if(ran == Result::ERROR)
				return false;
			if(ran != Result::RETURN)
				return Fail("function \"" + function.name + "\" ended without returning a value");
			result = variables[function.returnSlot];
			return true;
		}
		case Expression::Kind::HOST:
		{
			// Most functions take only a few arguments, so avoid allocating.
			const size_t count = expression.arguments.size();
			Value small[4];
			vector<Value> large(count > 4 ? count : 0);
			Value *values = count > 4 ? large.data() : small;
			for(size_t i = 0; i < count; ++i)
				if(!Evaluate(expression.arguments[i], variables, steps, values[i]))
					return false;
			string message;
			if(!interface->GetFunction(expression.function)(values, result, message))
				return Fail(message.empty() ? "a function failed" : message);
			return true;
		}
	}

	const vector<Expression> &arguments = expression.arguments;
	// "and", "or" and "if" only evaluate the arguments they need.
	if(expression.builtin == Builtin::AND || expression.builtin == Builtin::OR)
	{
		bool isAnd = (expression.builtin == Builtin::AND);
		for(const Expression &argument : arguments)
		{
			Value value;
			if(!Evaluate(argument, variables, steps, value))
				return false;
			if(value.GetType() != Value::Type::BOOLEAN)
				return Fail(string(isAnd ? "\"and\"" : "\"or\"") + " needs true or false values");
			if(value.Boolean() != isAnd)
			{
				result = Value(!isAnd);
				return true;
			}
		}
		result = Value(isAnd);
		return true;
	}
	if(expression.builtin == Builtin::IF)
	{
		Value condition;
		if(!Evaluate(arguments[0], variables, steps, condition))
			return false;
		if(condition.GetType() != Value::Type::BOOLEAN)
			return Fail("\"if\" needs a true or false condition");
		return Evaluate(arguments[condition.Boolean() ? 1 : 2], variables, steps, result);
	}

	// Keep the first few arguments on the stack; only "+", "*", "min", "max",
	// "and" and "or" take more than four, and rarely.
	const size_t count = arguments.size();
	Value values[4];
	vector<Value> more(count > 4 ? count - 4 : 0);
	auto argument = [&values, &more](size_t i) -> Value & { return i < 4 ? values[i] : more[i - 4]; };
	for(size_t i = 0; i < count; ++i)
		if(!Evaluate(arguments[i], variables, steps, argument(i)))
			return false;
	const Value &a = values[0];
	const Value &b = values[1];
	using Type = Value::Type;
	auto isNumbers = [&a, &b]() { return a.GetType() == Type::NUMBER && b.GetType() == Type::NUMBER; };
	auto isVectors = [&a, &b]() { return a.GetType() == Type::VECTOR && b.GetType() == Type::VECTOR; };
	auto typeError = [this](const char *function) {
		return Fail(string("wrong type of argument to \"") + function + "\"");
	};

	switch(expression.builtin)
	{
		case Builtin::ADD:
		{
			if(isNumbers())
				result = Value(a.Number() + b.Number());
			else if(isVectors())
				result = Value(a.Vector() + b.Vector());
			else
				return typeError("+");
			for(size_t i = 2; i < count; ++i)
			{
				const Value &value = argument(i);
				if(result.GetType() == Type::NUMBER && value.GetType() == Type::NUMBER)
					result = Value(result.Number() + value.Number());
				else if(result.GetType() == Type::VECTOR && value.GetType() == Type::VECTOR)
					result = Value(result.Vector() + value.Vector());
				else
					return typeError("+");
			}
			return true;
		}
		case Builtin::SUBTRACT:
			if(arguments.size() == 1)
			{
				if(a.GetType() == Type::NUMBER)
					result = Value(-a.Number());
				else if(a.GetType() == Type::VECTOR)
					result = Value(-a.Vector());
				else
					return typeError("-");
			}
			else if(isNumbers())
				result = Value(a.Number() - b.Number());
			else if(isVectors())
				result = Value(a.Vector() - b.Vector());
			else
				return typeError("-");
			return true;
		case Builtin::MULTIPLY:
			// Multiply from left to right, so (* a b c) is exactly (* (* a b) c).
			result = a;
			for(size_t i = 1; i < count; ++i)
			{
				const Value &value = argument(i);
				if(result.GetType() == Type::NUMBER && value.GetType() == Type::NUMBER)
					result = Value(result.Number() * value.Number());
				else if(result.GetType() == Type::NUMBER && value.GetType() == Type::VECTOR)
					result = Value(result.Number() * value.Vector());
				else if(result.GetType() == Type::VECTOR && value.GetType() == Type::NUMBER)
					result = Value(result.Vector() * value.Number());
				else
					return typeError("*");
			}
			return true;
		case Builtin::DIVIDE:
			if(isNumbers())
				result = Value(a.Number() / b.Number());
			else if(a.GetType() == Type::VECTOR && b.GetType() == Type::NUMBER)
				result = Value(a.Vector() / b.Number());
			else
				return typeError("/");
			return true;
		case Builtin::LESS:
		case Builtin::GREATER:
		case Builtin::LESS_EQUAL:
		case Builtin::GREATER_EQUAL:
			if(!isNumbers())
				return typeError("comparison");
			if(expression.builtin == Builtin::LESS)
				result = Value(a.Number() < b.Number());
			else if(expression.builtin == Builtin::GREATER)
				result = Value(a.Number() > b.Number());
			else if(expression.builtin == Builtin::LESS_EQUAL)
				result = Value(a.Number() <= b.Number());
			else
				result = Value(a.Number() >= b.Number());
			return true;
		case Builtin::EQUAL:
		case Builtin::NOT_EQUAL:
		{
			bool equal = false;
			if(isNumbers())
				equal = (a.Number() == b.Number());
			else if(a.GetType() == Type::BOOLEAN && b.GetType() == Type::BOOLEAN)
				equal = (a.Boolean() == b.Boolean());
			else if(isVectors())
				equal = (a.Vector() == b.Vector());
			else
				return typeError(expression.builtin == Builtin::EQUAL ? "==" : "!=");
			result = Value(expression.builtin == Builtin::EQUAL ? equal : !equal);
			return true;
		}
		case Builtin::NOT:
			if(a.GetType() != Type::BOOLEAN)
				return typeError("not");
			result = Value(!a.Boolean());
			return true;
		case Builtin::ABS:
		case Builtin::SQRT:
		case Builtin::ASIN:
		case Builtin::ACOS:
		case Builtin::SIN:
		case Builtin::COS:
		{
			if(a.GetType() != Type::NUMBER)
				return typeError("math function");
			double x = a.Number();
			double y = 0.;
			switch(expression.builtin)
			{
				case Builtin::ABS:
					y = fabs(x);
					break;
				case Builtin::SQRT:
					y = sqrt(x);
					break;
				case Builtin::ASIN:
					y = asin(x);
					break;
				case Builtin::ACOS:
					y = acos(x);
					break;
				case Builtin::SIN:
					y = sin(x);
					break;
				default:
					y = cos(x);
					break;
			}
			result = Value(y);
			return true;
		}
		case Builtin::ATAN2:
		case Builtin::MIN:
		case Builtin::MAX:
		case Builtin::COPYSIGN:
			if(!isNumbers())
				return typeError("math function");
			if(expression.builtin == Builtin::ATAN2)
				result = Value(atan2(a.Number(), b.Number()));
			else if(expression.builtin == Builtin::MIN || expression.builtin == Builtin::MAX)
			{
				// Like "*", from left to right.
				const bool isMin = (expression.builtin == Builtin::MIN);
				double extreme = isMin ? min(a.Number(), b.Number()) : max(a.Number(), b.Number());
				for(size_t i = 2; i < count; ++i)
				{
					const Value &value = argument(i);
					if(value.GetType() != Type::NUMBER)
						return typeError("math function");
					extreme = isMin ? min(extreme, value.Number()) : max(extreme, value.Number());
				}
				result = Value(extreme);
			}
			else
				result = Value(copysign(a.Number(), b.Number()));
			return true;
		case Builtin::SIGNBIT:
		case Builtin::IS_NAN:
			if(a.GetType() != Type::NUMBER)
				return typeError(expression.builtin == Builtin::SIGNBIT ? "signbit" : "is-nan");
			result = Value(expression.builtin == Builtin::SIGNBIT ? static_cast<bool>(signbit(a.Number()))
				: static_cast<bool>(isnan(a.Number())));
			return true;
		case Builtin::CLAMP:
		{
			const Value &c = values[2];
			if(!isNumbers() || c.GetType() != Type::NUMBER)
				return typeError("clamp");
			// A value that is not a number stays that way.
			double x = a.Number();
			if(x < b.Number())
				x = b.Number();
			else if(x > c.Number())
				x = c.Number();
			result = Value(x);
			return true;
		}
		case Builtin::VEC:
			if(!isNumbers())
				return typeError("vec");
			result = Value(Point(a.Number(), b.Number()));
			return true;
		case Builtin::X:
		case Builtin::Y:
		case Builtin::UNIT:
		case Builtin::LENGTH:
			if(a.GetType() != Type::VECTOR)
				return typeError("vector function");
			if(expression.builtin == Builtin::X)
				result = Value(a.Vector().X());
			else if(expression.builtin == Builtin::Y)
				result = Value(a.Vector().Y());
			else if(expression.builtin == Builtin::UNIT)
				result = Value(a.Vector().Unit());
			else
				result = Value(a.Vector().Length());
			return true;
		case Builtin::DOT:
		case Builtin::CROSS:
			if(!isVectors())
				return typeError("vector function");
			result = Value(expression.builtin == Builtin::DOT
				? a.Vector().Dot(b.Vector()) : a.Vector().Cross(b.Vector()));
			return true;
		case Builtin::RANDOM:
			result = Value(currentTape ? currentTape->Draw() : Random::Real());
			return true;
		case Builtin::RANDOM_INT:
		{
			// A whole number from 0 up to (but not including) the given count.
			const double n = a.GetType() == Type::NUMBER ? a.Number() : 0.;
			if(!(n >= 1. && n <= 4294967295.) || n != floor(n))
				return Fail("\"random-int\" needs a whole number from 1 to 4294967295");
			const uint32_t modulus = static_cast<uint32_t>(n);
			result = Value(currentTape ? currentTape->Draw(modulus) : static_cast<double>(Random::Int(modulus)));
			return true;
		}
		case Builtin::AND:
		case Builtin::OR:
		case Builtin::IF:
			break;
	}
	return Fail("unknown function");
}



void Program::Implementation::Report(const DataNode &node, const string &message)
{
	if(logErrors)
		node.PrintTrace(message);
	// Drop the colon that introduces the trace.
	string text = message;
	if(!text.empty() && text.back() == ':')
		text.pop_back();
	errors.push_back({node.LineNumber(), text});
}



bool Program::Implementation::Fail(const string &message) const
{
	if(currentTrace && currentTrace->error.empty())
	{
		currentTrace->error = message;
		currentTrace->errorLine = currentLine;
	}
	if(!reportedError)
		Logger::Log("Script \"" + name + "\" stopped: " + message + ".", Logger::Level::WARNING);
	reportedError = true;
	return false;
}



Program::Program()
	: implementation(make_unique<Implementation>())
{
}

Program::~Program() = default;
Program::Program(Program &&) noexcept = default;
Program &Program::operator=(Program &&) noexcept = default;



void Program::Load(const DataNode &node, const Interface &interface, bool logErrors)
{
	if(node.Size() >= 2)
		implementation->name = node.Token(1);
	implementation->logErrors = logErrors;
	implementation->Compile(node, interface);
}



const vector<Program::Error> &Program::Errors() const
{
	return implementation->errors;
}



bool Program::IsValid() const
{
	return implementation->valid;
}



const string &Program::Name() const
{
	return implementation->name;
}



int Program::SlotCount() const
{
	return implementation->slotCount;
}



bool Program::IsLinear() const
{
	return implementation->linear;
}



bool Program::Reads(int variable) const
{
	const auto &reads = implementation->reads;
	return variable >= 0 && variable < static_cast<int>(reads.size()) && reads[variable];
}



bool Program::Writes(int variable) const
{
	const auto &writes = implementation->writes;
	return variable >= 0 && variable < static_cast<int>(writes.size()) && writes[variable];
}



bool Program::Triggers(int effect) const
{
	const auto &triggers = implementation->triggers;
	return effect >= 0 && effect < static_cast<int>(triggers.size()) && triggers[effect];
}



bool Program::Run(vector<Value> &variables, uint32_t &effects, Memory *memory, Trace *trace) const
{
	if(trace)
		trace->Clear();
	Trace *previous = currentTrace;
	currentTrace = trace;
	traceDepth = 0;
	bool ran = implementation->Run(variables, effects, memory);
	currentTrace = previous;
	return ran;
}



void Trace::Clear()
{
	steps.clear();
	error.clear();
	errorLine = 0;
}



RandomTape::Use::Use(RandomTape *tape)
	: previous(currentTape)
{
	currentTape = tape;
}



RandomTape::Use::~Use()
{
	currentTape = previous;
}



void RandomTape::Record()
{
	entries.clear();
	position = 0;
	isPlaying = false;
	matches = true;
}



void RandomTape::Play()
{
	position = 0;
	isPlaying = true;
	matches = true;
}



bool RandomTape::Matches() const
{
	return matches;
}



bool RandomTape::IsFinished() const
{
	return position == entries.size();
}



double RandomTape::Draw(uint32_t modulus)
{
	if(!isPlaying)
	{
		const double value = modulus ? static_cast<double>(Random::Int(modulus)) : Random::Real();
		entries.push_back({modulus, value});
		return value;
	}
	if(position < entries.size() && entries[position].modulus == modulus)
		return entries[position++].value;
	// A different draw than before: what it gets no longer matters.
	matches = false;
	return 0.;
}



Activity::Watch::Watch(Activity *activity)
	: previous(currentActivity)
{
	currentActivity = activity;
}



Activity::Watch::~Watch()
{
	currentActivity = previous;
}



Activity *Activity::Current()
{
	return currentActivity;
}



Trace *Activity::Begin(const string &kind, const string &name)
{
	if(!currentActivity)
		return nullptr;
	for(Run &run : currentActivity->runs)
		if(run.kind == kind && run.name == name)
			return &run.trace;
	currentActivity->runs.push_back({kind, name, Trace()});
	return &currentActivity->runs.back().trace;
}



void Activity::Clear()
{
	runs.clear();
	notes.clear();
	overriddenBy.clear();
}



void Memory::Clear()
{
	values.clear();
	isSet.clear();
	slots.clear();
	resume.clear();
	waitLeft = 0;
	started = false;
	finished = false;
}



bool Memory::IsFinished() const
{
	return finished;
}

} // namespace script
