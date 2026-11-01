/* DefenseDaemon.h
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

#include <memory>
#include <string>
#include <utility>
#include <vector>

class DataNode;
class Projectile;
class Ship;



// A script that decides which incoming missile a ship's code injector hacks,
// loaded from a "daemon" node in the game data. Whenever the injector is
// ready, the game runs the script once for each homing missile chasing the
// ship that its code scanner can read, and hacks the one it scores highest,
// if that score is above zero. A ship runs a daemon if it has a Defense
// Daemon outfit (the player's flagship), or if its faction writes one (see
// Firmware), and either way it needs code hacking power to hack with.
class DefenseDaemon {
public:
	// Variables, in the order they are added to the interface. Everything
	// before SCORE is read-only.
	enum Variable {
		// The ship's own position and velocity, and how full its shields and
		// hull are (from 0 to 1).
		POSITION,
		VELOCITY,
		SHIELDS,
		HULL,
		// The missile: where it is and how it moves, relative to the ship.
		MISSILE_POSITION,
		MISSILE_VELOCITY,
		MISSILE_OFFSET,
		MISSILE_DISTANCE,
		// How fast it is closing in, and so about how many steps away it is.
		CLOSING_SPEED,
		STEPS_AWAY,
		// The damage it does to shields and hull, how many steps it has left
		// to live, whether its seeker sees the ship, whether it is confused,
		// and how many more steps it is hacked for.
		SHIELD_DAMAGE,
		HULL_DAMAGE,
		STEPS_LEFT,
		LOCKED,
		CONFUSED,
		HACKED,
		// Whether it is chasing the ship running the daemon (rather than
		// another ship in its fleet).
		CHASING_YOU,
		// Output: how much the missile should be hacked, or 0 (or less) not to.
		SCORE
	};

	class Input {
	public:
		Point position;
		Point velocity;
		double shields = 0.;
		double hull = 0.;
		Point missilePosition;
		Point missileVelocity;
		double shieldDamage = 0.;
		double hullDamage = 0.;
		int stepsLeft = 0;
		bool locked = false;
		bool confused = false;
		int hacked = 0;
		bool chasingYou = true;
	};

	static const script::Interface &GetInterface();
	// The input for a missile chasing the given ship (whose position and
	// state the inputs are relative to).
	static Input MakeInput(const Ship &ship, const Projectile &missile);


public:
	void Load(const DataNode &node, bool logErrors = true);

	const std::string &Name() const;
	bool IsValid() const;
	const script::Program &Program() const;

	// The missile's score. Returns false if the script fails.
	bool Score(const Input &input, double &score, script::Trace *trace = nullptr) const;
	// The missile among those given (each with the ship it is chasing) that
	// this daemon would hack, or null, run by the given ship.
	std::pair<const Ship *, Projectile *> Choose(const std::vector<std::pair<const Ship *, Projectile *>> &missiles,
		const Ship *runner) const;

	// The daemon the game uses when nobody has chosen another.
	static const DefenseDaemon *Factory();
	// The player's daemon: a program from their library, or one they bought,
	// as the text of a "daemon" node. Empty text (or one that doesn't
	// compile) means the factory's. Returns whether the text compiled.
	static bool SetPlayerProgram(const std::string &text);
	static const DefenseDaemon *PlayerProgram();


private:
	script::Program program;
};
