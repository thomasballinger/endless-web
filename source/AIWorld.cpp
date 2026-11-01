/* AIWorld.cpp
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

#include "AIWorld.h"

#include "AI.h"
#include "Command.h"
#include "DataFile.h"
#include "DataNode.h"
#include "Flotsam.h"
#include "GameData.h"
#include "Minable.h"
#include "Personality.h"
#include "Projectile.h"
#include "Ship.h"
#include "System.h"
#include "Visual.h"

#include <sstream>

using namespace std;

namespace {
	Personality MakePersonality(const string &words)
	{
		Personality personality;
		istringstream in("personality " + words + "\n");
		DataFile file(in);
		for(const DataNode &node : file)
			personality.Load(node);
		return personality;
	}
}



AIWorld::AIWorld(const string &systemName)
{
	system = GameData::Systems().Find(systemName);
	if(!system && !GameData::Systems().empty())
		system = &GameData::Systems().begin()->second;
	if(system)
		player.SetSystem(*system);
	ai = make_unique<AI>(player, ships, minables, flotsam);
}



AIWorld::~AIWorld() = default;



void AIWorld::AddPlayerShip(const shared_ptr<Ship> &ship)
{
	if(system)
		ship->SetSystem(system);
	ship->Recharge();
	player.CaptureShip(ship);
	ships.push_back(ship);
}



void AIWorld::AddAIShip(const shared_ptr<Ship> &ship, const string &personality)
{
	if(system)
		ship->SetSystem(system);
	ship->Recharge();
	ship->SetPersonality(MakePersonality(personality));
	ships.push_back(ship);
	flown.push_back(ship);
}



void AIWorld::Step()
{
	if(!system)
		return;
	Command commands;
	ai->Step(commands);
	// Only the AI's ships move here; the simulator moves the player's.
	vector<Visual> visuals;
	for(const shared_ptr<Ship> &ship : flown)
		ship->Move(visuals, flotsam);
	// Firing is also how the game notes a ship is in the player's system (so
	// that it stays a target); the shots themselves go nowhere.
	vector<Projectile> shots;
	for(const shared_ptr<Ship> &ship : ships)
		ship->Fire(shots, visuals, nullptr);
}
