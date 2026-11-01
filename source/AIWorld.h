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

#include "GuidanceDrill.h"
#include "PlayerInfo.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

class AI;
class Command;
class Flotsam;
class Minable;
class Ship;
class System;



// A small piece of the game for the simulators: a system with a few ships in
// it, which move exactly as ships do in the game (turning, thrusting, drag,
// energy and heat), but don't fire. Each ship is flown by the game's own AI,
// or by commands the simulator gives it each step (as a pilot would), or not
// at all (the simulator sets where it is). The first of the player's ships
// stands in for the player's flagship. Make a new world to start over: the
// AI remembers things from step to step.
class AIWorld {
public:
	enum class Pilot { AI, COMMANDS, FIXED };

	// The command a pilot gives for a drill's control: its turn, and thrust if
	// it asks for any.
	static Command ToCommand(const GuidanceDrill::Control &control);

	// The world is in the named system (or, if there is none, the first).
	explicit AIWorld(const std::string &system = "Sol");
	~AIWorld();
	AIWorld(const AIWorld &) = delete;
	AIWorld &operator=(const AIWorld &) = delete;

	// Add one of the player's ships (the first is the flagship), flown by
	// commands given to it.
	void AddPlayerShip(const std::shared_ptr<Ship> &ship);
	// Add a ship, in its own government. One the AI flies has a personality,
	// given as its words (e.g. "heroic staying").
	void AddShip(const std::shared_ptr<Ship> &ship, Pilot pilot, const std::string &personality = "");
	void AddAIShip(const std::shared_ptr<Ship> &ship, const std::string &personality);
	void Remove(const std::shared_ptr<Ship> &ship);
	// A step is: the AI decides what its ships do; then the simulator gives
	// the ships it flies their commands (Ship::SetCommands); then they move.
	void Think();
	void Move();


private:
	const System *system = nullptr;
	PlayerInfo player;
	std::list<std::shared_ptr<Ship>> ships;
	std::list<std::shared_ptr<Minable>> minables;
	std::list<std::shared_ptr<Flotsam>> flotsam;
	// The ships that move (all but those on a fixed course).
	std::vector<std::shared_ptr<Ship>> moving;
	std::unique_ptr<AI> ai;
};
