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

class Value {
public:
	enum class Type : uint8_t { NUMBER, BOOLEAN, VECTOR };

	Value();
	explicit Value(double number);
	explicit Value(bool boolean);
	explicit Value(const Point &vector);

	Type GetType() const;
	double Number() const;
	bool Boolean() const;
	const Point &Vector() const;

	static const char *TypeName(Type type);


private:
	Type type = Type::NUMBER;
	double number = 0.;
	bool boolean = false;
	Point vector;
};



// The variables and effects a host provides to programs.
class Interface {
public:
	// Variables are numbered in the order they are added; a program's first
	// variables are always the interface's, in the same order.
	int AddInput(const std::string &name);
	int AddOutput(const std::string &name);
	int AddEffect(const std::string &name);

	int VariableCount() const;
	int EffectCount() const;
	const std::string &VariableName(int variable) const;
	const std::string &EffectName(int effect) const;
	int Find(const std::string &name) const;
	bool IsOutput(int variable) const;
	int FindEffect(const std::string &name) const;


private:
	std::vector<std::string> variables;
	std::vector<bool> outputs;
	std::vector<std::string> effects;
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
	// that was triggered. Returns false, and stops, on a runtime error.
	bool Run(std::vector<Value> &variables, uint32_t &effects) const;


private:
	class Implementation;
	std::unique_ptr<Implementation> implementation;
};

} // namespace script
