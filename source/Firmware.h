/* Firmware.h
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
#include <vector>

class DataNode;
class Government;
class Weapon;



// A program that someone in the galaxy wrote: a guidance script for missiles,
// or a maneuver for escorts, that the player can buy for their library, and
// that the factions who wrote it fly. Loaded from "program" nodes; the script
// itself is a "guidance" or "maneuver" node of the same name elsewhere:
//
//   program "Corkscrew"
//   	guidance                     (or maneuver "keep station", for what it replaces)
//   	author "Pirate"
//   	"flown by" "Pirate" "Marauder"
//   	price 30000
//   	description `...`
//
// A program is sold where its author's government owns the planet (and
// everywhere, to a pilot who started in the Missile Lab).
class Firmware {
public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	bool IsGuidance() const;
	// For a maneuver, the maneuver it replaces (e.g. "keep station").
	const std::string &Replaces() const;
	const std::string &Author() const;
	int64_t Price() const;
	const std::string &Description() const;
	// Whether ships of this government fly it (or a government it is a
	// variant of: "Pirate (Rival)" counts as "Pirate").
	bool IsFlownBy(const Government *government) const;
	bool IsSoldBy(const Government *government) const;

	// The guidance program the given government's missiles fly instead of the
	// game's for the given weapon, if any (and if it can run on the weapon's
	// hardware).
	static const Firmware *GuidanceFor(const Government *government, const Weapon &weapon);

	// For the text of a program: its kind ("guidance" or "maneuver", from its
	// first node) and its name, or empty strings if it isn't one.
	static std::string KindOf(const std::string &text);
	static std::string NameOf(const std::string &text);
	// The text with its first node renamed.
	static std::string Renamed(const std::string &text, const std::string &name);
	// Whether the game itself has a script of this kind and name (a factory
	// program, or one of these), as opposed to one only the player has.
	static bool IsGameScript(const std::string &kind, const std::string &name);


private:
	std::string name;
	bool isGuidance = true;
	std::string replaces;
	std::string author;
	std::vector<std::string> flownBy;
	int64_t price = 0;
	std::string description;
};
