/* CodeStyle.h
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

#include "Color.h"

#include <map>
#include <string>
#include <vector>

class DataNode;
class Government;



// How one faction's code looks when the player watches it run: its colors,
// the comments its programmers put at the top of everything, and for alien
// code, a cipher that turns its words into a script the player can't read
// (though the numbers still show). Loaded from "code style" nodes:
//
//   "code style" "Pirate"
//   	government "Pirate" "Marauder"
//   	header "# ripped out of a dead merchant's flight computer"
//   	color keyword .9 .5 .3
//   	cipher "zyxwvutsrqponmlkjihgfedcba"
//
// A style applies to the governments it names, and to their variants: "Hai"
// also covers "Hai Merchant" and "Hai (Unfettered)".
class CodeStyle {
public:
	void Load(const DataNode &node);

	// The style for a government's code, or null for the plain one.
	static const CodeStyle *ForGovernment(const Government *government);

	const std::string &Name() const;
	// Whether this style is for the given government.
	bool Covers(const std::string &government) const;
	// Comment lines to show above any code in this style.
	const std::vector<std::string> &Header() const;
	// The color for the given part of the code ("keyword", "name", "number",
	// "comment", "string", "paren", "background", "lit"), or null to use the
	// usual one.
	const Color *GetColor(const std::string &part) const;
	// Text as this style shows it: with a cipher, every letter is replaced.
	std::string Encode(const std::string &text) const;
	bool HasCipher() const;


private:
	std::string name;
	std::vector<std::string> governments;
	std::vector<std::string> header;
	std::map<std::string, Color> colors;
	// The letter to show for each of "a" through "z", or empty.
	std::string cipher;
};
