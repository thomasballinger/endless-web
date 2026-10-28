/* Script.h
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

#include "../Point.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class DataNode;



// A small scripting language for game logic such as missile guidance.
//
// Programs are written in the game's data file format. Each line is an
// expression in prefix notation, and may use parentheses for nested
// expressions; indented lines below a line form its block:
//
//     let d (- target-position position)
//     if (< (dot d facing) 0)
//         set turn 0
//     elif has-lock
//         set turn (* to-deg (asin (cross facing (unit d))))
//     else
//         drop-target
//
// There are no loops or user-defined functions, so every run takes a bounded
// number of steps. Values are numbers, booleans, and 2D vectors; vector math
// uses the same Point operations as the game's C++ code, so a script can
// reproduce built-in behavior exactly.
//
// The game decides what a program can see and do by declaring an Interface:
// read-only inputs (sensor readings), outputs the program may set, and
// effects (actions with no arguments) it may trigger.
namespace script {

class RecordList;

class Value {
public:
	// A LIST is a list of records the host gave the script, and a RECORD is
	// one of them. Scripts can only loop over lists and read records' fields.
	enum class Type : uint8_t { NUMBER, BOOLEAN, VECTOR, LIST, RECORD };

	Value();
	explicit Value(double number);
	explicit Value(bool boolean);
	explicit Value(const Point &vector);
	explicit Value(const RecordList &list);
	Value(const RecordList &list, size_t record);

	Type GetType() const;
	double Number() const;
	bool Boolean() const;
	const Point &Vector() const;
	// For a list or record: the list.
	const RecordList *List() const;
	// For a record: its index in the list.
	size_t Record() const;

	static const char *TypeName(Type type);


private:
	Type type = Type::NUMBER;
	double number = 0.;
	bool boolean = false;
	Point vector;
	const RecordList *list = nullptr;
	size_t record = 0;
};



// A list of records, such as the flotsam near a ship, that a host gives to a
// script as an input. Every record has the same fields, which the host names
// when it adds the list to its interface. A script can only read the list
// while it runs, so the host may reuse it afterward.
class RecordList {
public:
	explicit RecordList(int fieldCount);

	void Clear();
	// Add a record. Set its fields with SetField().
	void Add();
	void SetField(int field, const Value &value);

	size_t Size() const;
	const Value &Field(size_t record, int field) const;


private:
	int fieldCount = 0;
	std::vector<Value> values;
};



// The variables and effects a host provides to programs.
class Interface {
public:
	// Variables are numbered in the order they are added; a program's first
	// variables are always the interface's, in the same order.
	int AddInput(const std::string &name);
	// Add an input that is a list of records with the given fields, which
	// scripts can loop over with "for".
	int AddList(const std::string &name, const std::vector<std::string> &fields);
	int AddOutput(const std::string &name);
	int AddEffect(const std::string &name);
	// A function the host provides, such as a game calculation scripts may
	// use. It is called with an array of the given number of arguments, and
	// returns false (after setting an error message) if they are not valid.
	using Function = std::function<bool(const Value *arguments, Value &result, std::string &error)>;
	int AddFunction(const std::string &name, int argumentCount, Function function);

	int VariableCount() const;
	int EffectCount() const;
	const std::string &VariableName(int variable) const;
	const std::string &EffectName(int effect) const;
	int Find(const std::string &name) const;
	bool IsOutput(int variable) const;
	bool IsList(int variable) const;
	// The names of the fields of a list's records, or -1 if there is no such field.
	int FindField(int list, const std::string &field) const;
	int FindEffect(const std::string &name) const;
	int FindFunction(const std::string &name) const;
	int FunctionArgumentCount(int function) const;
	const Function &GetFunction(int function) const;


private:
	std::vector<std::string> variables;
	std::vector<bool> outputs;
	// For each list input, the names of its records' fields.
	std::vector<std::vector<std::string>> listFields;
	std::vector<std::string> effects;
	std::vector<std::string> functionNames;
	std::vector<int> functionArgumentCounts;
	std::vector<Function> functions;
};



// Values that a program keeps from one run to the next ("memory" statements),
// for one object such as one projectile.
class Memory {
public:
	void Clear();

private:
	friend class Program;
	std::vector<Value> values;
	std::vector<bool> isSet;
	// Which compiled version of the program these values are for.
	uint64_t version = 0;
};



class Program {
public:
	Program();
	~Program();
	Program(Program &&) noexcept;
	Program &operator=(Program &&) noexcept;

	// Compile the children of the given node as the program's statements.
	// Errors are reported with node.PrintTrace() and make the program invalid.
	void Load(const DataNode &node, const Interface &interface);
	bool IsValid() const;
	const std::string &Name() const;

	// Number of variable slots Run() needs, including the interface's.
	int SlotCount() const;

	// Whether the program reads or sets one of the interface's variables, or
	// triggers one of its effects, anywhere in its code. A host can use these
	// to only allow a program where it has the capabilities the program uses.
	bool Reads(int variable) const;
	bool Writes(int variable) const;
	bool Triggers(int effect) const;

	// Run the program. The first Interface::VariableCount() entries of
	// variables hold inputs and outputs; effects receives one bit per effect
	// that was triggered. The values of "memory" statements are kept in the
	// given memory between runs; with none, they start over every run.
	// Returns false, and stops, on a runtime error.
	bool Run(std::vector<Value> &variables, uint32_t &effects, Memory *memory = nullptr) const;


private:
	class Implementation;
	std::unique_ptr<Implementation> implementation;
};

} // namespace script
