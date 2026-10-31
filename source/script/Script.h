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
	// A short description of the value, for showing to a player.
	std::string ToString() const;


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



// The random numbers that runs of programs draw. While a tape is in use on a
// thread, the "random" and "random-int" builtins either note each number they
// draw on it, or, while it plays back, get the numbers noted on it instead, in
// order. That lets another version of a program run on exactly what an
// earlier run saw.
class RandomTape {
public:
	// Use the given tape (or none) on this thread until this is destroyed.
	class Use {
	public:
		explicit Use(RandomTape *tape);
		~Use();
		Use(const Use &) = delete;
		Use &operator=(const Use &) = delete;

	private:
		RandomTape *previous = nullptr;
	};


public:
	// Start over with an empty tape, noting what is drawn.
	void Record();
	// Play back what was noted, from the start.
	void Play();

	// While playing back: whether every draw so far was the same kind as the
	// one noted in its place, and whether every noted draw has been used.
	bool Matches() const;
	bool IsFinished() const;

	// A number from [0, 1), or with a modulus, a whole number below it.
	double Draw(uint32_t modulus = 0);


private:
	class Entry {
	public:
		uint32_t modulus = 0;
		double value = 0.;
	};

	std::vector<Entry> entries;
	size_t position = 0;
	bool isPlaying = false;
	bool matches = true;
};



// What happened during one run of a program, for showing a player how their
// script ran: each statement executed, in order, with its line in the data
// file and its result.
class Trace {
public:
	class Step {
	public:
		enum class Kind {
			// A "let", "set" or "memory" gave a variable a value.
			ASSIGN,
			// An "if" or "elif" condition was checked; value is true or false.
			CONDITION,
			// An effect was triggered.
			EFFECT,
			// The program stopped.
			STOP,
			// A function returned a value.
			RETURN,
			// A "for" loop moved on to the record in value (its index).
			LOOP
		};

		Kind kind = Kind::ASSIGN;
		size_t line = 0;
		// The variable or effect involved, if any.
		std::string name;
		Value value;
		// How many function calls deep the statement was.
		int depth = 0;
	};

	void Clear();

	std::vector<Step> steps;
	// The runtime error that stopped the program, if any.
	std::string error;
	// The line where it happened.
	size_t errorLine = 0;
};



// The scripts that ran for one object (such as a ship) during a step, for
// showing a player. While an activity is being watched on a thread, every
// program run that asks for a trace with Begin() gets one in it.
class Activity {
public:
	// One script's latest run.
	class Run {
	public:
		// The kind of script, as its data file names it ("maneuver", "turret",
		// "guidance" and so on), and its name.
		std::string kind;
		std::string name;
		Trace trace;
	};

	// Note runs on this thread in the given activity (or in none) until this
	// is destroyed.
	class Watch {
	public:
		explicit Watch(Activity *activity);
		~Watch();
		Watch(const Watch &) = delete;
		Watch &operator=(const Watch &) = delete;

	private:
		Activity *previous = nullptr;
	};


public:
	// The activity being noted on this thread, if any.
	static Activity *Current();
	// Where to trace a run of the given script, or null if nothing is being
	// watched. A script that runs more than once in a step keeps its latest.
	static Trace *Begin(const std::string &kind, const std::string &name);

	void Clear();

	std::vector<Run> runs;
	// Anything else worth telling about the step, such as which AI rules ran.
	std::vector<std::string> notes;
	// If not empty, why the object's scripts were overridden (e.g. a confused
	// seeker steering instead of its guidance script).
	std::string overriddenBy;
};



class Program {
public:
	// A mistake that keeps a program from compiling, and the line of the data
	// file where it is.
	class Error {
	public:
		size_t line = 0;
		std::string message;
	};


public:
	Program();
	~Program();
	Program(Program &&) noexcept;
	Program &operator=(Program &&) noexcept;

	// Compile the children of the given node as the program's statements.
	// Errors are reported with node.PrintTrace() (unless logErrors is false,
	// e.g. while a player is typing) and make the program invalid.
	void Load(const DataNode &node, const Interface &interface, bool logErrors = true);
	bool IsValid() const;
	// The errors found when the program was loaded.
	const std::vector<Error> &Errors() const;
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
	// Returns false, and stops, on a runtime error. With a trace, records each
	// statement that ran (which is slower, so only for showing a player).
	bool Run(std::vector<Value> &variables, uint32_t &effects, Memory *memory = nullptr,
		Trace *trace = nullptr) const;


private:
	class Implementation;
	std::unique_ptr<Implementation> implementation;
};

} // namespace script
