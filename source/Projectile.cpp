/* Projectile.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Projectile.h"

#include "Collision.h"
#include "CollisionType.h"
#include "Effect.h"
#include "Entity.h"
#include "FighterHitHelper.h"
#include "GameData.h"
#include "Logger.h"
#include "Minable.h"
#include "MissileGuidance.h"
#include "pi.h"
#include "Random.h"
#include "Ship.h"
#include "Visual.h"
#include "Weapon.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace {
	// How often a seeker that has lost its lock gets it back, compared to
	// keeping one it has.
	const double RELOCK_RATE = .3;

	// Get the current power of jamming at a specified range.
	double RangeFraction(double distance, double jamming)
	{
		return 1. - min(1., distance / Projectile::JammingRange(jamming));
	}

	// Given the probability of losing a lock in five tries, check randomly
	// whether it should be lost on this try.
	inline bool Check(double probability, double base)
	{
		return (Random::Real() < base * probability);
	}

	// Returns if the missile is confused or not.
	bool ConfusedTracking(double tracking, double jamming, double distance)
	{
		if(!jamming)
			return Random::Real() > tracking;
		else
		{
			jamming = sqrt(jamming) * RangeFraction(distance, jamming);
			return Random::Real() > tracking / (1. + jamming);
		}
	}
}



Projectile::Projectile(const Ship &parent, Point position, Angle angle, const Weapon *weapon)
	: Body(weapon->ProjectileSprite(), position, parent.Velocity(), angle),
	weapon(weapon), lifetime(weapon->Lifetime())
{
	// Projectiles get their own random numbers, seeded by whatever made them.
	Random::SeedStream(RandomStream());
	shared_ptr<Ship> targetShip = parent.GetTargetShip();
	target = targetShip ? static_pointer_cast<Entity>(targetShip)
		: static_pointer_cast<Entity>(parent.GetTargetAsteroid());

	government = parent.GetGovernment();
	hitsRemaining = weapon->PenetrationCount();
	// The player's own missiles fly the guidance the player programmed.
	if(parent.IsYours() && weapon->Homing())
		playerGuidance = MissileGuidance::PlayerProgram(weapon);

	// If you are boarding your target, do not fire on it.
	if(parent.IsBoarding() || parent.Commands().Has(Command::BOARD))
		target.reset();

	cachedTarget = TargetPtr().get();
	if(cachedTarget)
	{
		targetGovernment = cachedTarget->GetGovernment();
		targetDisabled = cachedTarget->IsDisabled();
	}

	dV = this->angle.Unit() * (weapon->Velocity() + Random::Real() * weapon->RandomVelocity());
	velocity += dV;

	// If a random lifetime is specified, add a random amount up to that amount.
	if(weapon->RandomLifetime())
		lifetime += Random::Int(weapon->RandomLifetime() + 1);

	// Set an initial confusion turn direction.
	if(weapon->Homing() && cachedTarget)
	{
		confusionDirection = Random::Int(2) ? -1 : 1;
		CheckLock(*cachedTarget);
		CheckConfused(*cachedTarget);
	}
}



Projectile::Projectile(const Projectile &parent, const Point &offset, const Angle &angle, const Weapon *weapon)
	: Body(weapon->ProjectileSprite(), parent.position + parent.velocity + parent.angle.Rotate(offset),
	parent.velocity, parent.angle + angle),
	weapon(weapon), target(parent.target), lifetime(weapon->Lifetime())
{
	// Projectiles get their own random numbers, seeded by whatever made them.
	Random::SeedStream(RandomStream());
	government = parent.government;
	targetGovernment = parent.targetGovernment;
	targetDisabled = parent.targetDisabled;
	hitsRemaining = weapon->PenetrationCount();

	cachedTarget = TargetPtr().get();

	// Given that submunitions inherit the velocity of the parent projectile,
	// it is often the case that submunitions don't add any additional velocity.
	// But we still want inaccuracy to have an effect on submunitions. Because of
	// this, we tilt the velocity of submunitions in the direction of the inaccuracy.
	dV = this->angle.Unit() * (parent.dV.Length() + weapon->Velocity() + Random::Real() * weapon->RandomVelocity());
	velocity += dV - parent.dV;

	// If a random lifetime is specified, add a random amount up to that amount.
	if(weapon->RandomLifetime())
		lifetime += Random::Int(weapon->RandomLifetime() + 1);

	// Set an initial confusion turn direction.
	if(weapon->Homing() && cachedTarget)
	{
		confusionDirection = Random::Int(2) ? -1 : 1;
		CheckLock(*cachedTarget);
		CheckConfused(*cachedTarget);
	}
}



// Ship explosion.
Projectile::Projectile(const Point &position, const Angle &angle, const Weapon *weapon)
	: weapon(weapon), isShipExplosion(true)
{
	// Projectiles get their own random numbers, seeded by whatever made them.
	Random::SeedStream(RandomStream());
	this->position = position;
	this->angle = angle;
}



namespace {
	// When ES_MISSILE_STATS is set, count how the projectiles of each homing
	// weapon end, and print the counts when the game exits.
	struct Outcomes {
		uint64_t hit = 0;
		uint64_t expired = 0;
		uint64_t shotDown = 0;
	};
	map<const Weapon *, Outcomes> outcomes;

	void PrintOutcomes()
	{
		map<string, Outcomes> byName;
		for(const auto &[name, outfit] : GameData::Outfits())
		{
			auto it = outcomes.find(outfit.GetWeapon().get());
			if(it != outcomes.end())
				byName[name] = it->second;
		}
		for(const auto &[name, counts] : byName)
			printf("MISSILES %s|hit=%llu expired=%llu shotdown=%llu\n", name.c_str(),
				static_cast<unsigned long long>(counts.hit), static_cast<unsigned long long>(counts.expired),
				static_cast<unsigned long long>(counts.shotDown));
		fflush(stdout);
	}

	void RecordOutcome(const Weapon &weapon, int death)
	{
		static const bool collect = getenv("ES_MISSILE_STATS");
		if(!collect || !weapon.Homing())
			return;
		if(outcomes.empty())
			atexit(PrintOutcomes);
		Outcomes &counts = outcomes[&weapon];
		if(death & (Projectile::COLLISION | Projectile::EXPLOSION))
			++counts.hit;
		else if(death & Projectile::ANTI_MISSILE)
			++counts.shotDown;
		else
			++counts.expired;
	}
}



// This returns false if it is time to delete this projectile.
void Projectile::Move(vector<Visual> &visuals, vector<Projectile> &projectiles)
{
	Random::Scope randomScope(RandomStream());
	++age;
	if(hackedSteps)
		--hackedSteps;
	if(--lifetime <= 0)
	{
		if(death == DeathType::NATURAL || death == DeathType::ANTI_MISSILE)
		{
			// Create any death effects.
			// Place effects ahead of the projectile by 1.5x velocity. 1x comes from
			// the anticipated movement of the projectile on its frame of death, and
			// 0.5x comes from the behavior of BatchDrawList::Add drawing the projectile sprite
			// half way between its current position and its next position.
			Point effectPosition = position + 1.5 * velocity;
			for(const auto &it : weapon->DieEffects())
				for(int i = 0; i < it.second; ++i)
					visuals.emplace_back(*it.first, effectPosition, velocity, angle);
		}

		for(const auto &it : weapon->Submunitions())
		{
			if(!(it.spawnOn & death))
				continue;
			const Weapon *const subWeapon = it.weapon.get();
			if(!subWeapon)
				continue;
			for(size_t i = 0; i < it.count; ++i)
			{
				Angle inaccuracy = Distribution::GenerateInaccuracy(subWeapon->Inaccuracy(),
						subWeapon->InaccuracyDistribution());
				projectiles.emplace_back(*this, it.offset, it.facing + inaccuracy, subWeapon);
			}
		}
		RecordOutcome(*weapon, death);
		MarkForRemoval();
		return;
	}
	// Spawn live effects. By using the current position of the projectile and not
	// adding any offset from the projectile's velocity, effects will appear to spawn
	// from behind the projectile, as by the time the effect is visible, the projectile
	// will have moved one frame forward from this position.
	for(const auto &it : weapon->LiveEffects())
		if(!Random::Int(it.second))
			visuals.emplace_back(*it.first, position, velocity, angle);

	// If the target has left the system, stop following it. Also stop if the
	// target has been captured by a different government.
	// Also stop targeting fighters that have become disabled after this projectile was fired.
	const Entity *currentTarget = cachedTarget;
	if(currentTarget)
	{
		currentTarget = TargetPtr().get();
		if(!currentTarget || !currentTarget->IsTargetable() || currentTarget->GetGovernment() != targetGovernment
			|| (currentTarget->EntityType() == Entity::Type::SHIP && !targetDisabled
				&& !FighterHitHelper::IsValidTarget(static_cast<const Ship *>(currentTarget))))
		{
			BreakTarget();
			currentTarget = nullptr;
		}
	}

	double turn = weapon->Turn();
	double accel = weapon->Acceleration();
	bool homing = weapon->Homing();
	if(currentTarget && homing && !Random::Int(30))
	{
		++lockChecks;
		CheckLock(*currentTarget);
		CheckConfused(*currentTarget);
	}
	if(turn)
	{
		// Update the confusion direction after the projectile turns about
		// 180 degrees away from its target.
		if(!Random::Int(ceil(180 / turn)))
			confusionDirection = Random::Int(2) ? -1 : 1;
		// A homing projectile's guidance script decides how it steers.
		if(homing)
			Steer(currentTarget, turn, accel);

		if(turn)
			angle += Angle(turn);
	}

	if(accel)
	{
		double d = 1. - weapon->Drag();
		Point a = accel * angle.Unit();
		velocity *= d;
		velocity += a;
		dV *= d;
		dV += a;
	}

	position += velocity;
	// Only measure the distance that this projectile traveled under its own
	// power, as opposed to including any velocity that came from the firing
	// ship.
	distanceTraveled += dV.Length();

	// If this projectile is now within its "split range," it should split into
	// sub-munitions next turn.
	if(currentTarget && (position - currentTarget->Position()).Length() < weapon->SplitRange())
		lifetime = 0;

	// A projectile will begin to fade out when the remaining lifetime is smaller
	// than the specified "fade out" time.
	if(lifetime < weapon->FadeOut())
		alpha = static_cast<double>(lifetime) / weapon->FadeOut();
}


// Run this projectile's guidance script to decide how far to turn this step,
// and how much to accelerate.
void Projectile::Steer(const Entity *currentTarget, double &turn, double &accel)
{
	ranGuidance = false;
	// The seeker itself steers in two cases, and then the guidance script
	// does not run that step (and a trace of it shows nothing):
	// A seeker with a blind spot can't see a target behind the projectile, so
	// it loses the target, and the projectile turns at its full turn rate, as
	// it always has.
	if(currentTarget && weapon->HasBlindspot()
			&& (currentTarget->Position() - position).Dot(angle.Unit()) < 0.)
	{
		if(guidanceTrace)
			guidanceTrace->Clear();
		target.reset();
		return;
	}
	// A seeker that has lost its lock and is confused turns the projectile as
	// fast as it can, one way or the other, and so does a hacked one.
	if(hackedSteps || (isConfused && !(currentTarget && hasLock)))
	{
		if(guidanceTrace)
			guidanceTrace->Clear();
		if(script::Activity *activity = script::Activity::Current())
			activity->overriddenBy = hackedSteps ? "HACKED: its seeker is steering, not the script"
				: "SEEKER CONFUSED: it is steering, not the script";
		turn *= confusionDirection;
		return;
	}

	const MissileGuidance *guidance = playerGuidance ? playerGuidance : weapon->Guidance();
	if(guidance && guidance->IsValid())
	{
		string missing = guidance->MissingCapability(*weapon);
		if(!missing.empty())
		{
			// Only complain once about each script and missing capability.
			static set<string> warned;
			if(warned.insert(guidance->Name() + missing).second)
				Logger::Log("A weapon can't use guidance \"" + guidance->Name() + "\" because " + missing
					+ ". Weapons without that use their default guidance instead.", Logger::Level::WARNING);
			guidance = nullptr;
		}
	}
	if(!guidance)
		guidance = MissileGuidance::Default(*weapon);
	// A plugin could replace a default script with one that needs more than
	// the weapons using it have.
	if(!guidance || !guidance->IsValid() || !guidance->MissingCapability(*weapon).empty())
	{
		static bool warned = false;
		if(!warned)
			Logger::Log("A homing weapon has no valid \"guidance\" script that it is able to use;"
				" its projectiles will not steer.", Logger::Level::ERROR);
		warned = true;
		turn = 0.;
		return;
	}

	MissileGuidance::Input input;
	input.position = position;
	input.velocity = velocity;
	input.facing = angle.Unit();
	input.hasLock = currentTarget && hasLock;
	if(input.hasLock)
	{
		input.targetPosition = currentTarget->Position();
		// An imperfect seeker misjudges the bearing to its target by a
		// different amount on each step.
		if(weapon->SeekerNoise())
			input.targetPosition = position
				+ Angle(Random::Normal(0., weapon->SeekerNoise())).Rotate(input.targetPosition - position);
		input.targetVelocity = currentTarget->Velocity();
		input.targetFacing = currentTarget->Facing().Unit();
	}
	input.maxTurn = turn;
	input.acceleration = accel;
	input.drag = weapon->Drag();
	input.age = age;
	input.lifetimeLeft = lifetime;

	MissileGuidance::Output output;
	ranGuidance = true;
	if(guidanceTrace)
		lastGuidanceInput = input;
	MissileGuidance::Record *record = nullptr;
	if(guidanceRecords)
	{
		record = &guidanceRecords->emplace_back();
		record->input = input;
		record->draws.Record();
	}
	bool succeeded;
	{
		script::RandomTape::Use use(record ? &record->draws : nullptr);
		succeeded = guidance->Run(input, output, &guidanceMemory, guidanceTrace);
	}
	if(record)
	{
		record->succeeded = succeeded;
		record->output = output;
	}
	if(!succeeded)
	{
		turn = 0.;
		return;
	}
	if(output.dropTarget)
		target.reset();
	turn = output.turn;
	accel = output.thrust;
}



void Projectile::SetGuidanceTrace(script::Trace *trace)
{
	guidanceTrace = trace;
}



const MissileGuidance::Input &Projectile::LastGuidanceInput() const
{
	return lastGuidanceInput;
}



void Projectile::SetWatchedBy(const Ship *ship)
{
	watchedBy = ship;
}



const Ship *Projectile::WatchedBy() const
{
	return watchedBy;
}



void Projectile::Hack(int steps)
{
	hackedSteps = max(hackedSteps, steps);
}



bool Projectile::HasLock() const
{
	return hasLock && weapon->Homing() && TargetPtr();
}



string Projectile::ConfusionCause() const
{
	if(confusingJamming == 3)
		return "radar and optical jamming";
	if(confusingJamming == 2)
		return "optical jamming";
	if(confusingJamming == 1)
		return "radar jamming";
	return "poor tracking";
}



bool Projectile::IsConfused() const
{
	return isConfused && !hasLock;
}



void Projectile::SetGuidanceRecords(vector<MissileGuidance::Record> *records)
{
	guidanceRecords = records;
}



// This projectile hit something. Create the explosion, if any. This also
// marks the projectile as needing deletion if it has run out of hits.
void Projectile::Collide(vector<Visual> &visuals, const Collision &collision)
{
	// Offset the placement position of effects by the projectile's velocity while
	// also accounting for the intersection clipping. Hit effects should appear from
	// the front of the projectile, and so are shifted forward by the full velocity
	// of the projectile.
	double intersection = collision.IntersectionRange();
	Point effectPosition = position + velocity * intersection;
	Point hitVelocity = collision.HitVelocity();
	for(const auto &[effect, count] : weapon->HitEffects())
		for(int i = 0; i < count; ++i)
			visuals.emplace_back(*effect, effectPosition, velocity, angle, hitVelocity);
	// The projectile dies if it has no hits remaining.
	if(--hitsRemaining == 0)
	{
		clip = intersection;
		lifetime = 0;
		death = collision.GetCollisionType() == CollisionType::EXPLOSION ? DeathType::EXPLOSION : DeathType::COLLISION;
	}
}



// Get the amount of clipping that should be applied when drawing this projectile.
double Projectile::Clip() const
{
	return clip;
}



// Get whether the lifetime on this projectile has run out.
bool Projectile::IsDead() const
{
	return lifetime <= 0;
}



// This projectile was killed, e.g. by an anti-missile system.
void Projectile::Kill()
{
	lifetime = 0;
	death = DeathType::ANTI_MISSILE;
}



// Find out if this is a missile, and if so, how strong it is (i.e. what
// chance an anti-missile shot has of destroying it).
int Projectile::MissileStrength() const
{
	return weapon->MissileStrength();
}



// Get information on the weapon that fired this projectile.
const Weapon &Projectile::GetWeapon() const
{
	return *weapon;
}



// Get information on how this projectile impacted a ship.
Projectile::ImpactInfo Projectile::GetInfo(double intersection) const
{
	// Account for the distance that this projectile traveled before intersecting
	// with the target.
	return ImpactInfo(*weapon, position + velocity * intersection, distanceTraveled + dV.Length() * intersection);
}



// Find out which entity this projectile is targeting.
const Entity *Projectile::Target() const
{
	return cachedTarget;
}



const Government *Projectile::TargetGovernment() const
{
	return targetGovernment;
}



shared_ptr<Entity> Projectile::TargetPtr() const
{
	return target.lock();
}



// Clear the targeting information on this projectile.
void Projectile::BreakTarget()
{
	target.reset();
	cachedTarget = nullptr;
	targetGovernment = nullptr;
	targetDisabled = false;
}



// TODO: add more conditions in the future. For example maybe proximity to stars
// and their brightness could could cause IR missiles to lose their locks more
// often, and dense asteroid fields could do the same for radar and optically
// guided missiles.
void Projectile::CheckLock(const Entity &target)
{
	double base = hasLock ? 1. : RELOCK_RATE;
	hasLock = false;
	// Each tracking method this weapon has gets a try (drawing a random number
	// whether or not an earlier one succeeded).
	for(double probability : LockProbabilities(target, position.Distance(target.Position())))
		hasLock |= Check(probability, base);
}



// For each tracking type, the probability twice every second that a lock will be
// lost, for the tracking types this projectile's weapon has, in the order they
// are checked.
vector<double> Projectile::LockProbabilities(const Entity &target, double distance) const
{
	vector<double> probabilities;
	auto add = [&probabilities](double lockChance) {
		probabilities.push_back(lockChance / (RELOCK_RATE - (lockChance * RELOCK_RATE) + lockChance));
	};
	if(weapon->Tracking())
		add(weapon->Tracking());

	// Optical tracking is about 1.5% for an average interceptor (250 mass),
	// about 50% for an average medium warship (1000 mass),
	// and about 95% for an average heavy warship (2500 mass).
	// Optical jamming decreases the effective size of a ship.
	if(weapon->OpticalTracking())
	{
		double opticalJamming = target.IsDisabled() ? 0. : target.OpticalJamming();
		if(opticalJamming)
			opticalJamming *= RangeFraction(distance, opticalJamming);
		double opticalSize = target.OpticalSize() / (1. + opticalJamming);
		double weight = opticalSize * opticalSize * opticalSize / 1e9;
		add(weapon->OpticalTracking() * weight / (1. + weight));
	}

	// Infrared tracking is zero when heat is zero and 100% when heat is full.
	// When the missile is at under 1/3 of its maximum range, tracking is
	// linearly increased by up to a factor of 3, representing the fact that the
	// wavelengths of IR radiation are easier to distinguish at closer distances.
	if(weapon->InfraredTracking())
	{
		double shortRange = weapon->Range() * 0.33;
		double multiplier = 1.;
		if(distance <= shortRange)
			multiplier = 2. - distance / shortRange;
		add(weapon->InfraredTracking() * min(1., target.HeatFraction() * multiplier));
	}

	// Radar tracking depends on whether the target ship has jamming capabilities.
	// The jamming effect attenuates with range, and that range is affected by
	// the power of the jamming. Jamming of 2 will cause a sidewinder fired at
	// least 1200 units away from a maneuvering target to miss about 25% of the
	// time. Jamming of 10 will increase that to about 60%.
	if(weapon->RadarTracking())
	{
		double radarJamming = target.IsDisabled() ? 0. : target.RadarJamming();
		if(radarJamming)
			radarJamming *= RangeFraction(distance, radarJamming);
		add(weapon->RadarTracking() / (1. + radarJamming));
	}
	return probabilities;
}



double Projectile::LockChance() const
{
	const shared_ptr<Entity> target = TargetPtr();
	if(!target)
		return 0.;
	return LockChanceAt(position.Distance(target->Position()), hasLock);
}



double Projectile::LockChanceAt(double distance, bool locked) const
{
	const shared_ptr<Entity> target = TargetPtr();
	if(!target || !weapon->Homing())
		return 0.;
	const double base = locked ? 1. : RELOCK_RATE;
	double miss = 1.;
	for(double probability : LockProbabilities(*target, distance))
		miss *= 1. - min(1., base * probability);
	return 1. - miss;
}



MissileGuidance::Input Projectile::CurrentGuidanceInput() const
{
	MissileGuidance::Input input;
	input.position = position;
	input.velocity = velocity;
	input.facing = angle.Unit();
	const shared_ptr<Entity> target = TargetPtr();
	input.hasLock = HasLock();
	if(input.hasLock && target)
	{
		input.targetPosition = target->Position();
		input.targetVelocity = target->Velocity();
		input.targetFacing = target->Facing().Unit();
	}
	input.maxTurn = weapon->Turn();
	input.acceleration = weapon->Acceleration();
	input.drag = weapon->Drag();
	input.age = age;
	input.lifetimeLeft = lifetime;
	return input;
}



bool Projectile::RanGuidance() const
{
	return ranGuidance;
}



int Projectile::LockChecks() const
{
	return lockChecks;
}



vector<Projectile::LockMethod> Projectile::LockBreakdown() const
{
	vector<LockMethod> methods;
	const shared_ptr<Entity> target = TargetPtr();
	if(!target || !weapon->Homing())
		return methods;
	const double distance = position.Distance(target->Position());
	const double base = hasLock ? 1. : RELOCK_RATE;
	char text[96];
	// In the order LockProbabilities() gives them.
	if(weapon->Tracking())
		methods.push_back({"tracking", weapon->Tracking(), "", 0.});
	if(weapon->OpticalTracking())
	{
		const double jamming = target->IsDisabled() ? 0. : target->OpticalJamming();
		if(jamming && distance < JammingRange(jamming))
			snprintf(text, sizeof(text), "size %.0f, made smaller by jamming %.1f", target->OpticalSize(),
				jamming * RangeFraction(distance, jamming));
		else
			snprintf(text, sizeof(text), "target size %.0f", target->OpticalSize());
		methods.push_back({"optical", weapon->OpticalTracking(), text, 0.});
	}
	if(weapon->InfraredTracking())
	{
		const double shortRange = weapon->Range() * 0.33;
		if(distance <= shortRange)
			snprintf(text, sizeof(text), "target heat %.0f%%, x%.1f up close", 100. * target->HeatFraction(),
				2. - distance / shortRange);
		else
			snprintf(text, sizeof(text), "target heat %.0f%%", 100. * target->HeatFraction());
		methods.push_back({"infrared", weapon->InfraredTracking(), text, 0.});
	}
	if(weapon->RadarTracking())
	{
		const double jamming = target->IsDisabled() ? 0. : target->RadarJamming();
		if(jamming && distance < JammingRange(jamming))
			snprintf(text, sizeof(text), "jammed %.1f (of %g, up close)", jamming * RangeFraction(distance, jamming),
				jamming);
		else if(jamming)
			snprintf(text, sizeof(text), "out of its jamming's reach");
		else
			snprintf(text, sizeof(text), "no jamming");
		methods.push_back({"radar", weapon->RadarTracking(), text, 0.});
	}
	const vector<double> probabilities = LockProbabilities(*target, distance);
	for(size_t i = 0; i < methods.size() && i < probabilities.size(); ++i)
		methods[i].chance = min(1., base * probabilities[i]);
	return methods;
}



double Projectile::JammingRange(double jamming)
{
	return 500. + sqrt(jamming) * 500.;
}



// Homing weapons that have lost their lock have a chance to get confused
// and turn in a random direction ("go haywire"). Each tracking method has
// a different haywire condition. Weapons with multiple tracking methods
// only go haywire if all of the tracking methods have gotten confused.
void Projectile::CheckConfused(const Entity &target)
{
	if(hasLock)
	{
		isConfused = false;
		return;
	}

	bool trackingConfused = true;
	bool infraredConfused = true;
	bool opticalConfused = true;
	bool radarConfused = true;

	// Tracking and Infrared: proportional to tracking quality.
	if(weapon->Tracking())
		trackingConfused = Random::Real() > weapon->Tracking();

	if(weapon->InfraredTracking())
		infraredConfused = Random::Real() > weapon->InfraredTracking();

	// Optical and Radar: If the target has no jamming, then proportional to tracking
	// quality. If the target does have jamming, then it's proportional to
	// tracking quality, the strength of target's jamming, and the distance
	// to the target (jamming power attenuates with distance).
	double distance = position.Distance(target.Position());
	if(weapon->OpticalTracking())
	{
		double opticalTracking = weapon->OpticalTracking();
		double opticalJamming = target.OpticalJamming();
		opticalConfused = ConfusedTracking(opticalTracking, opticalJamming, distance);
	}

	if(weapon->RadarTracking())
	{
		double radarTracking = weapon->RadarTracking();
		double radarJamming = target.RadarJamming();
		radarConfused = ConfusedTracking(radarTracking, radarJamming, distance);
	}

	isConfused = trackingConfused && infraredConfused && opticalConfused && radarConfused;
	// Remember what to blame (without drawing any more random numbers).
	confusingJamming = 0;
	if(isConfused)
	{
		if(weapon->RadarTracking() && target.RadarJamming())
			confusingJamming |= 1;
		if(weapon->OpticalTracking() && target.OpticalJamming())
			confusingJamming |= 2;
	}
}



double Projectile::DistanceTraveled() const
{
	return distanceTraveled;
}



bool Projectile::Phases(const Ship &ship) const
{
	return phasedShip == &ship;
}



uint16_t Projectile::HitsRemaining() const
{
	return hitsRemaining;
}



void Projectile::SetPhases(const Ship *ship)
{
	phasedShip = ship;
}



bool Projectile::ShouldExplode() const
{
	return isShipExplosion || (weapon->IsFused() && lifetime == 1);
}
