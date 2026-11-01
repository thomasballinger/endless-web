/* AIWorld.h
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

#include "PlayerInfo.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

class AI;
class Flotsam;
class Minable;
class Ship;
class System;



// A small piece of the game for the simulators: a system with a few ships in
// it, some of which the game's own AI flies, as it flies ships in the game.
// One ship stands in for the player's flagship; the simulator flies it (and
// any others it adds as the player's) itself, by setting where they are each
// step. The AI's ships move as ships do in the game, but don't fire. Make a
// new world to start over: the AI remembers things from step to step.
class AIWorld {
public:
	// The world is in the named system (or, if there is none, the first).
	explicit AIWorld(const std::string &system = "Sol");
	~AIWorld();
	AIWorld(const AIWorld &) = delete;
	AIWorld &operator=(const AIWorld &) = delete;

	// Add one of the player's ships (the first is the flagship).
	void AddPlayerShip(const std::shared_ptr<Ship> &ship);
	// Add a ship for the AI to fly, with a personality given as its words
	// (e.g. "heroic staying"), in its own government.
	void AddAIShip(const std::shared_ptr<Ship> &ship, const std::string &personality);
	// Let the AI decide what its ships do, then move them.
	void Step();


private:
	const System *system = nullptr;
	PlayerInfo player;
	std::list<std::shared_ptr<Ship>> ships;
	std::list<std::shared_ptr<Minable>> minables;
	std::list<std::shared_ptr<Flotsam>> flotsam;
	std::vector<std::shared_ptr<Ship>> flown;
	std::unique_ptr<AI> ai;
};
