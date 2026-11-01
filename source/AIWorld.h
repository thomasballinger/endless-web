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

#include "CollisionSet.h"
#include "GuidanceDrill.h"
#include "PlayerInfo.h"
#include "Projectile.h"
#include "ShipEvent.h"

#include <list>
#include <memory>
#include <set>
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
// energy and heat), and fight as they do: they fire as the game's AI decides,
// their shots fly and hit as the game's do, and do the damage they do in the
// game, so ships lose shields and hull, are disabled, and are destroyed. Each
// ship is flown by the game's own AI, or by commands the simulator gives it
// each step (as a pilot would), or not at all (the simulator sets where it
// is). The first of the player's ships stands in for the player's flagship
// (which fires only as the "Automatic firing" preference has it). Make a new
// world to start over: the AI remembers things from step to step. (Carriers
// don't launch their fighters here.)
class AIWorld {
public:
	enum class Pilot { AI, COMMANDS, FIXED };
	// What became of a projectile the simulator checked with Strike().
	enum class Outcome { NOTHING, HIT, SHOT_DOWN };

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
	// Fly a ship this step as the game's AI flies a ship that attacks its
	// target on its own (as an escort does when its role says to attack).
	void FlyIndependent(const std::shared_ptr<Ship> &ship);
	// Have a ship hold its fire (or not), whatever the AI decides.
	void HoldFire(const std::shared_ptr<Ship> &ship, bool hold = true);
	// A step is: the AI decides what its ships do; then the simulator gives
	// the ships it flies their commands (Ship::SetCommands); then they move,
	// fire, and their shots move and hit. A ship that is destroyed leaves the
	// world once it has finished exploding.
	void Think();
	void Move();

	// Check a projectile the simulator moves itself (say, a missile it is
	// testing), after it moves, for hits on the world's ships, as the game
	// does: it deals its damage, or the ships' anti-missile shoots it down.
	Outcome Strike(Projectile &projectile);
	// The shots the world's ships fired that are still flying.
	const std::vector<Projectile> &Projectiles() const;


private:
	const System *system = nullptr;
	PlayerInfo player;
	std::list<std::shared_ptr<Ship>> ships;
	std::list<std::shared_ptr<Minable>> minables;
	std::list<std::shared_ptr<Flotsam>> flotsam;
	// The ships that move (all but those on a fixed course), and those that
	// hold their fire.
	std::vector<std::shared_ptr<Ship>> moving;
	std::set<const Ship *> holding;
	std::unique_ptr<AI> ai;
	int step = 0;
	std::vector<Projectile> projectiles;
	CollisionSet shipCollisions;
	// The ships whose anti-missile is ready this step.
	std::vector<Ship *> hasAntiMissile;
	// What happened since the AI last thought (who attacked whom...).
	std::list<ShipEvent> events;
};
