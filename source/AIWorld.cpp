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
#include "Collision.h"
#include "Command.h"
#include "DamageDealt.h"
#include "DamageProfile.h"
#include "DataFile.h"
#include "DataNode.h"
#include "FighterHitHelper.h"
#include "FireCommand.h"
#include "Flotsam.h"
#include "GameData.h"
#include "Government.h"
#include "image/Mask.h"
#include "Minable.h"
#include "Personality.h"
#include "Random.h"
#include "Ship.h"
#include "System.h"
#include "Visual.h"
#include "Weapon.h"

#include <algorithm>
#include <map>
#include <sstream>

using namespace std;

namespace {
	// A hit that is still to do its damage (as in Engine::EntityCollision).
	class Hit {
	public:
		Hit(const DamageProfile &damage, const Government *gov, bool provokable = true)
			: damage(damage), gov(gov), provokable(provokable) {}

		DamageProfile damage;
		const Government *gov;
		bool provokable;
	};

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
	: shipCollisions(256u, 32u, CollisionType::SHIP)
{
	system = GameData::Systems().Find(systemName);
	if(!system && !GameData::Systems().empty())
		system = &GameData::Systems().begin()->second;
	if(system)
		player.SetSystem(*system);
	ai = make_unique<AI>(player, ships, minables, flotsam);
}



AIWorld::~AIWorld() = default;



Command AIWorld::ToCommand(const GuidanceDrill::Control &control)
{
	Command command;
	command.SetTurn(max(-1., min(1., control.turn)));
	if(control.thrust > 0.)
		command |= Command::FORWARD;
	return command;
}



void AIWorld::AddPlayerShip(const shared_ptr<Ship> &ship)
{
	if(system)
		ship->SetSystem(system);
	ship->Recharge();
	Random::SeedStream(ship->RandomStream());
	player.CaptureShip(ship);
	ships.push_back(ship);
	moving.push_back(ship);
}



void AIWorld::AddShip(const shared_ptr<Ship> &ship, Pilot pilot, const string &personality)
{
	if(system)
		ship->SetSystem(system);
	ship->Recharge();
	Random::SeedStream(ship->RandomStream());
	if(pilot == Pilot::AI)
		ship->SetPersonality(MakePersonality(personality));
	ships.push_back(ship);
	if(pilot != Pilot::FIXED)
		moving.push_back(ship);
}



void AIWorld::AddAIShip(const shared_ptr<Ship> &ship, const string &personality)
{
	AddShip(ship, Pilot::AI, personality);
}



void AIWorld::Remove(const shared_ptr<Ship> &ship)
{
	ships.remove(ship);
	moving.erase(remove(moving.begin(), moving.end(), ship), moving.end());
	holding.erase(ship.get());
}



void AIWorld::FlyIndependent(const shared_ptr<Ship> &ship)
{
	if(!system)
		return;
	Command command;
	ai->FlyIndependent(*ship, command);
	ship->SetCommands(command);
}



void AIWorld::HoldFire(const shared_ptr<Ship> &ship, bool hold)
{
	if(hold)
		holding.insert(ship.get());
	else
		holding.erase(ship.get());
}



void AIWorld::Think()
{
	if(!system)
		return;
	// The AI hears what happened (who attacked whom) before it decides.
	ai->UpdateEvents(events);
	events.clear();
	Command commands;
	ai->Step(commands);
}



void AIWorld::Move()
{
	if(!system)
		return;
	++step;
	// As in Engine::MoveShip: each ship moves, then fires. (What its
	// explosions leave behind, the AI never sees.)
	vector<Visual> visuals;
	list<shared_ptr<Flotsam>> debris;
	vector<Projectile> fired;
	hasAntiMissile.clear();
	for(const shared_ptr<Ship> &ship : ships)
	{
		Random::Scope randomScope(ship->RandomStream());
		ship->UpdateCaches();
		if(find(moving.begin(), moving.end(), ship) != moving.end())
		{
			ship->Move(visuals, debris);
			events.splice(events.end(), ship->HandleEvents());
			if(ship->ShouldBeRemoved())
				continue;
		}
		if(holding.contains(ship.get()))
		{
			FireCommand none = ship->FiringCommands();
			none.Clear();
			ship->SetCommands(none);
		}
		ship->Fire(fired, visuals, nullptr);
		if(ship->HasAntiMissile())
			hasAntiMissile.push_back(ship.get());
	}
	// Ships that have finished exploding are gone.
	vector<shared_ptr<Ship>> gone;
	for(const shared_ptr<Ship> &ship : ships)
		if(ship->ShouldBeRemoved())
			gone.push_back(ship);
	for(const shared_ptr<Ship> &ship : gone)
		Remove(ship);

	// Then the shots that were already flying move, and the new ones join
	// them (to move from the next step on).
	for(Projectile &projectile : projectiles)
		projectile.Move(visuals, fired);
	projectiles.erase(remove_if(projectiles.begin(), projectiles.end(),
		[](const Projectile &projectile) { return projectile.ShouldBeRemoved(); }), projectiles.end());
	projectiles.insert(projectiles.end(), make_move_iterator(fired.begin()), make_move_iterator(fired.end()));

	// Then everything checks for collisions along its next movement.
	shipCollisions.Clear(step);
	for(const shared_ptr<Ship> &ship : ships)
		if(ship->Zoom() == 1.)
			shipCollisions.Add(*ship);
	shipCollisions.Finish();
	for(Projectile &projectile : projectiles)
		Strike(projectile);
}



AIWorld::Outcome AIWorld::Strike(Projectile &projectile)
{
	if(!system || projectile.IsDead())
		return Outcome::NOTHING;
	// This follows Engine::FindCollisions and Engine::DoCollisions, for ships
	// only (there are no asteroids here).
	Random::Scope randomScope(projectile.RandomStream());
	vector<Visual> visuals;
	vector<Collision> collisions;
	const Government *gov = projectile.GetGovernment();
	const Weapon &weapon = projectile.GetWeapon();
	if(projectile.ShouldExplode())
		collisions.emplace_back(nullptr, CollisionType::EXPLOSION, 0.);
	else if(weapon.IsPhasing() && projectile.Target())
	{
		shared_ptr<Entity> target = projectile.TargetPtr();
		if(target && target->EntityType() == Entity::Type::SHIP)
		{
			Point offset = projectile.Position() - target->Position();
			double range = target->GetMask(step).Collide(offset, projectile.Velocity(), target->Facing());
			if(range < 1.)
				collisions.emplace_back(target.get(), CollisionType::SHIP, range);
		}
	}
	else
	{
		double triggerRadius = weapon.TriggerRadius();
		if(triggerRadius)
		{
			vector<Body *> inRadius;
			shipCollisions.Circle(projectile.Position(), triggerRadius, inRadius);
			for(const Body *body : inRadius)
			{
				const Ship *ship = static_cast<const Ship *>(body);
				if(body == projectile.Target() || ((!gov || gov->IsEnemy(body->GetGovernment()))
						&& !ship->IsCloaked() && FighterHitHelper::IsValidTarget(ship)))
				{
					collisions.emplace_back(nullptr, CollisionType::EXPLOSION, 0.);
					break;
				}
			}
		}
		if(collisions.empty() && weapon.CanCollideShips())
			shipCollisions.Line(projectile, collisions);
	}
	sort(collisions.begin(), collisions.end());

	map<Ship *, vector<Hit>> hits;
	bool hit = false;
	for(Collision &collision : collisions)
	{
		Body *body = collision.HitBody();
		const double range = collision.IntersectionRange();
		Ship *shipHit = (body && collision.GetCollisionType() == CollisionType::SHIP)
			? static_cast<Ship *>(body) : nullptr;
		if(shipHit && body != projectile.Target() && !FighterHitHelper::IsValidTarget(shipHit))
			continue;
		if(shipHit && shipHit->Phases(projectile))
			continue;

		projectile.Collide(visuals, collision);
		hit = true;
		Projectile::ImpactInfo impact = projectile.GetInfo(range);
		const double blastRadius = weapon.BlastRadius();
		if(blastRadius)
		{
			Point hitPos = projectile.Position() + range * projectile.Velocity();
			vector<Body *> blasted;
			shipCollisions.Circle(hitPos, blastRadius, blasted);
			for(Body *inBlast : blasted)
			{
				Ship *ship = static_cast<Ship *>(inBlast);
				bool targeted = (projectile.Target() == ship);
				if((weapon.IsSafe() && !targeted && gov && !gov->IsEnemy(ship->GetGovernment()))
						|| ship->Phases(projectile))
					continue;
				hits[ship].emplace_back(DamageProfile(*ship, impact, ship == body), gov, targeted);
			}
		}
		else if(shipHit)
			hits[shipHit].emplace_back(DamageProfile(*shipHit, impact), gov);
		if(projectile.IsDead())
			break;
	}

	// Shields take the damage first, so the hits that do most to them land
	// first (as in Engine::DoCollisions).
	for(auto &[ship, shipHits] : hits)
	{
		Random::Scope shipScope(ship->RandomStream());
		sort(shipHits.begin(), shipHits.end(), [](const Hit &a, const Hit &b) {
			const double aScale = a.damage.Scaling();
			const double bScale = b.damage.Scaling();
			const double aShield = a.damage.GetWeapon().ShieldDamage() * aScale;
			const double bShield = b.damage.GetWeapon().ShieldDamage() * bScale;
			if(aShield == bShield)
				return a.damage.GetWeapon().HullDamage() * aScale < b.damage.GetWeapon().HullDamage() * bScale;
			return aShield > bShield;
		});
		const shared_ptr<Ship> victim = ship->shared_from_this();
		for(const Hit &it : shipHits)
		{
			DamageDealt damage = it.damage.CalculateDamage();
			const int eventType = ship->TakeDamage(visuals, damage, it.provokable ? it.gov : nullptr);
			if(eventType)
				events.emplace_back(it.gov, victim, eventType);
		}
	}
	if(hit)
		return Outcome::HIT;

	// Anti-missile systems get a chance to shoot it down.
	if(projectile.MissileStrength())
		for(Ship *ship : hasAntiMissile)
			if(ship == projectile.Target() || !gov || gov->IsEnemy(ship->GetGovernment()))
				if(ship->FireAntiMissile(projectile, visuals))
				{
					projectile.Kill();
					return Outcome::SHOT_DOWN;
				}
	return Outcome::NOTHING;
}



const vector<Projectile> &AIWorld::Projectiles() const
{
	return projectiles;
}
