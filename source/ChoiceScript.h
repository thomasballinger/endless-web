/* ChoiceScript.h
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

#include "Point.h"
#include "script/Script.h"

#include <string>

class DataNode;



// A script that helps a ship choose one of several things, loaded from the
// game data. For now the only choice is which flotsam a ship chases to pick
// up, and there are two ways to write it, so they can be compared:
//  - A "rate" script looks at one candidate and gives it a score, or skips
//    it. The game runs it for each candidate and takes the lowest score.
//  - A "choose" script gets the whole list as "flotsam" and loops over it
//    with "for", choosing one itself.
// If a "choose" script named "flotsam" is defined, the AI uses it; otherwise
// it uses the "rate" script.
class ChoiceScript {
public:
	enum class Kind { RATE, CHOOSE };

	// Variables, in the order they are added to each interface. Both kinds
	// start with the ship's own position, velocity, facing (a unit vector),
	// top speed, turn rate, and whether it is the player's.
	enum Variable {
		POSITION,
		VELOCITY,
		FACING,
		MAX_VELOCITY,
		TURN_RATE,
		YOURS,
		// For "rate": the candidate's position and velocity, and the output
		// score. For "choose": the list of candidates (each with a position
		// and velocity), and the output index of the chosen one, or -1.
		CANDIDATE_POSITION = 6,
		CANDIDATE_VELOCITY = 7,
		SCORE = 8,
		CANDIDATES = 6,
		CHOSEN = 7
	};
	// "rate" scripts can skip a candidate.
	enum Effect {
		SKIP
	};

	class Input {
	public:
		Point position;
		Point velocity;
		Point facing;
		double maxVelocity = 0.;
		double turnRate = 0.;
		bool yours = false;
		// For "rate":
		Point candidatePosition;
		Point candidateVelocity;
		// For "choose": records with a position and a velocity.
		const script::RecordList *candidates = nullptr;
	};

	static const script::Interface &GetInterface(Kind kind);


public:
	void Load(const DataNode &node, Kind kind);

	const std::string &Name() const;
	bool IsValid() const;

	// For a "rate" script: the candidate's score. Returns false if the script
	// skips the candidate or fails.
	bool Rate(const Input &input, double &score) const;
	// For a "choose" script: the index of the chosen candidate, or -1.
	int Choose(const Input &input) const;


private:
	Kind kind = Kind::RATE;
	script::Program program;
};
