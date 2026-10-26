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

#include "../DataNode.h"
#include "../Logger.h"
#include "../pi.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

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
	}
	return "unknown";
}



int Interface::AddInput(const string &name)
{
	variables.push_back(name);
	outputs.push_back(false);
	return static_cast<int>(variables.size()) - 1;
}

int Interface::AddOutput(const string &name)
{
	variables.push_back(name);
	outputs.push_back(true);
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
	return -1;
}

bool Interface::IsOutput(int variable) const
{
	return variable >= 0 && variable < static_cast<int>(outputs.size()) && outputs[variable];
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
		VEC, X, Y, DOT, CROSS, UNIT, LENGTH
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
		{"*", {Builtin::MULTIPLY, 2, 2}},
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
		{"min", {Builtin::MIN, 2, 2}},
		{"max", {Builtin::MAX, 2, 2}},
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

		string atom;
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



class Program::Implementation {
public:
	class Expression {
	public:
		enum class Kind { CONSTANT, VARIABLE, CALL, HOST };
		Kind kind = Kind::CONSTANT;
		Value constant;
		int slot = -1;
		Builtin builtin = Builtin::ADD;
		// For HOST: which of the interface's functions to call.
		int function = -1;
		vector<Expression> arguments;
	};

	class Block;
	class Statement {
	public:
		enum class Kind { ASSIGN, IF, EFFECT, STOP, MEMORY };
		Kind kind = Kind::STOP;
		int slot = -1;
		Expression expression;
		int effect = -1;
		// For MEMORY: which of the program's memory values this is.
		int memory = -1;
		// For IF: the condition and block of the "if" and each "elif", then
		// an optional "else" block with no condition.
		vector<Expression> conditions;
		vector<Block> blocks;
		// For statistics.
		string description;
		size_t index = 0;
	};

	class Block {
	public:
		vector<Statement> statements;
	};


public:
	bool Compile(const DataNode &node, const Interface &interface);
	bool Run(vector<Value> &variables, uint32_t &effects, Memory *memory) const;


private:
	bool CompileBlock(const DataNode &node, Block &block);
	bool CompileExpression(const SExpression &source, const DataNode &node, Expression &expression);
	int Lookup(const string &name) const;

	enum class Result { CONTINUE, STOP, ERROR };
	Result RunBlock(const Block &block, vector<Value> &variables, uint32_t &effects, Memory *memory,
		int &steps) const;
	bool Evaluate(const Expression &expression, vector<Value> &variables, int &steps, Value &result) const;
	bool Fail(const string &message) const;
	void CountStatement(const Statement &statement) const;


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


private:
	const Interface *interface = nullptr;
	// Names visible at each nesting level while compiling.
	vector<map<string, int>> scopes;
	size_t statementCount = 0;
	mutable bool reportedError = false;
	mutable string error;
};



bool Program::Implementation::Compile(const DataNode &node, const Interface &interface)
{
	this->interface = &interface;
	slotCount = interface.VariableCount();
	reads.assign(slotCount, false);
	writes.assign(slotCount, false);
	triggers.assign(interface.EffectCount(), false);
	memorySlots.clear();
	scopes.assign(1, {});
	body = Block();
	valid = CompileBlock(node, body);
	scopes.clear();
	return valid;
}



bool Program::Implementation::CompileBlock(const DataNode &node, Block &block)
{
	scopes.emplace_back();
	bool ok = true;
	for(auto it = node.begin(); it != node.end(); ++it)
	{
		const DataNode &child = *it;
		SExpression line;
		string message = ReadLine(child, line);
		if(!message.empty())
		{
			child.PrintTrace(message);
			ok = false;
			continue;
		}
		const string &keyword = line.items.front().atom;
		const size_t argumentCount = line.items.size() - 1;

		Statement statement;
		statement.description = Describe(child);
		statement.index = statementCount++;
		if(keyword == "let" || keyword == "set" || keyword == "memory")
		{
			if(argumentCount != 2 || !line.items[1].IsAtom() || child.HasChildren())
			{
				child.PrintTrace("Expected \"" + keyword + " <name> <expression>\":");
				ok = false;
				continue;
			}
			const string &variable = line.items[1].atom;
			if(!CompileExpression(line.items[2], child, statement.expression))
			{
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
					child.PrintTrace("\"memory\" can only be used outside of any block:");
					ok = false;
					continue;
				}
				if(Lookup(variable) >= 0 || CONSTANTS.contains(variable))
				{
					child.PrintTrace("\"" + variable + "\" is already defined:");
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
				if(Lookup(variable) >= 0 || CONSTANTS.contains(variable))
				{
					child.PrintTrace("\"" + variable + "\" is already defined:");
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
					child.PrintTrace("Unknown variable \"" + variable + "\":");
					ok = false;
					continue;
				}
				if(statement.slot < interface->VariableCount() && !interface->IsOutput(statement.slot))
				{
					child.PrintTrace("\"" + variable + "\" is an input and can't be changed:");
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
				ReadLine(*branch, branchLine);
				const string &branchKeyword = branchLine.items.front().atom;
				bool isElse = (branchKeyword == "else");
				if(isElse ? branchLine.items.size() != 1 : branchLine.items.size() != 2)
				{
					branch->PrintTrace(isElse ? "Expected \"else\" alone on its line:"
						: "Expected \"" + branchKeyword + " <condition>\":");
					ok = false;
				}
				else if(!isElse)
				{
					statement.conditions.emplace_back();
					ok &= CompileExpression(branchLine.items[1], *branch, statement.conditions.back());
				}
				statement.blocks.emplace_back();
				ok &= CompileBlock(*branch, statement.blocks.back());

				auto next = branch;
				++next;
				if(isElse || next == node.end())
					break;
				SExpression nextLine;
				if(!ReadLine(*next, nextLine).empty())
					break;
				const string &nextKeyword = nextLine.items.front().atom;
				if(nextKeyword != "elif" && nextKeyword != "else")
					break;
				branch = next;
			}
			it = branch;
		}
		else if(keyword == "elif" || keyword == "else")
		{
			child.PrintTrace("\"" + keyword + "\" must follow an \"if\" or \"elif\":");
			ok = false;
			continue;
		}
		else if(keyword == "stop")
		{
			if(argumentCount || child.HasChildren())
			{
				child.PrintTrace("Expected \"stop\" alone on its line:");
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
				child.PrintTrace("Unknown statement \"" + keyword + "\":");
				ok = false;
				continue;
			}
			if(argumentCount || child.HasChildren())
			{
				child.PrintTrace("\"" + keyword + "\" takes no arguments:");
				ok = false;
				continue;
			}
			statement.kind = Statement::Kind::EFFECT;
			triggers[statement.effect] = true;
		}
		block.statements.push_back(std::move(statement));
	}
	scopes.pop_back();
	return ok;
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
		expression.kind = Expression::Kind::VARIABLE;
		expression.slot = Lookup(source.atom);
		if(expression.slot < 0)
		{
			node.PrintTrace("Unknown variable \"" + source.atom + "\":");
			return false;
		}
		if(expression.slot < interface->VariableCount())
			reads[expression.slot] = true;
		return true;
	}

	if(source.items.empty() || !source.items.front().IsAtom())
	{
		node.PrintTrace("An expression in parentheses must begin with a function name:");
		return false;
	}
	const string &function = source.items.front().atom;
	int count = static_cast<int>(source.items.size()) - 1;
	auto it = BUILTINS.find(function);
	if(it == BUILTINS.end())
	{
		// The host may provide functions of its own.
		int host = interface->FindFunction(function);
		if(host < 0)
		{
			node.PrintTrace("Unknown function \"" + function + "\":");
			return false;
		}
		if(count != interface->FunctionArgumentCount(host))
		{
			node.PrintTrace("Wrong number of arguments to \"" + function + "\":");
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
		node.PrintTrace("Wrong number of arguments to \"" + function + "\":");
		return false;
	}
	expression.kind = Expression::Kind::CALL;
	expression.builtin = it->second.builtin;
	expression.arguments.resize(count);
	bool ok = true;
	for(int i = 0; i < count; ++i)
		ok &= CompileExpression(source.items[i + 1], node, expression.arguments[i]);
	return ok;
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
		memory->values.resize(memorySlots.size());
		memory->isSet.resize(memorySlots.size(), false);
		for(size_t i = 0; i < memorySlots.size(); ++i)
			if(memory->isSet[i])
				variables[memorySlots[i]] = memory->values[i];
	}
	int steps = 0;
	if(RunBlock(body, variables, effects, memory, steps) == Result::ERROR)
		return false;
	if(memory)
		for(size_t i = 0; i < memorySlots.size(); ++i)
			if(memory->isSet[i])
				memory->values[i] = variables[memorySlots[i]];
	return true;
}



Program::Implementation::Result Program::Implementation::RunBlock(const Block &block, vector<Value> &variables,
	uint32_t &effects, Memory *memory, int &steps) const
{
	for(const Statement &statement : block.statements)
	{
		CountStatement(statement);
		switch(statement.kind)
		{
			case Statement::Kind::ASSIGN:
			{
				Value value;
				if(!Evaluate(statement.expression, variables, steps, value))
					return Result::ERROR;
				variables[statement.slot] = value;
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
					if(result != Result::CONTINUE)
						return result;
				}
				break;
			}
			case Statement::Kind::EFFECT:
				effects |= (1u << statement.effect);
				break;
			case Statement::Kind::STOP:
				return Result::STOP;
			case Statement::Kind::MEMORY:
			{
				// The initial value is only used on the first run.
				if(memory && memory->isSet[statement.memory])
					break;
				Value value;
				if(!Evaluate(statement.expression, variables, steps, value))
					return Result::ERROR;
				variables[statement.slot] = value;
				if(memory)
					memory->isSet[statement.memory] = true;
				break;
			}
		}
	}
	return Result::CONTINUE;
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
		case Expression::Kind::HOST:
		{
			vector<Value> values(expression.arguments.size());
			for(size_t i = 0; i < values.size(); ++i)
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

	Value values[2];
	vector<Value> extra;
	for(size_t i = 0; i < arguments.size(); ++i)
	{
		Value &value = (i < 2 ? values[i] : (extra.emplace_back(), extra.back()));
		if(!Evaluate(arguments[i], variables, steps, value))
			return false;
	}
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
			for(const Value &value : extra)
			{
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
			if(isNumbers())
				result = Value(a.Number() * b.Number());
			else if(a.GetType() == Type::NUMBER && b.GetType() == Type::VECTOR)
				result = Value(a.Number() * b.Vector());
			else if(a.GetType() == Type::VECTOR && b.GetType() == Type::NUMBER)
				result = Value(a.Vector() * b.Number());
			else
				return typeError("*");
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
			else if(expression.builtin == Builtin::MIN)
				result = Value(min(a.Number(), b.Number()));
			else if(expression.builtin == Builtin::MAX)
				result = Value(max(a.Number(), b.Number()));
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
			const Value &c = extra.front();
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
		case Builtin::AND:
		case Builtin::OR:
		case Builtin::IF:
			break;
	}
	return Fail("unknown function");
}



bool Program::Implementation::Fail(const string &message) const
{
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



void Program::Load(const DataNode &node, const Interface &interface)
{
	if(node.Size() >= 2)
		implementation->name = node.Token(1);
	implementation->Compile(node, interface);
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



bool Program::Run(vector<Value> &variables, uint32_t &effects, Memory *memory) const
{
	return implementation->Run(variables, effects, memory);
}



void Memory::Clear()
{
	values.clear();
	isSet.clear();
}

} // namespace script
