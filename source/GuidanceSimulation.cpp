/* GuidanceSimulation.cpp
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

#include "GuidanceSimulation.h"

#include "GameData.h"
#include "Government.h"
#include "image/Mask.h"
#include "MissileGuidance.h"
#include "Outfit.h"
#include "Random.h"
#include "Visual.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

namespace {
	// Fire a projectile this often, in steps.
	const int LAUNCH_INTERVAL = 90;
	// How many recent positions to keep for each projectile's trail.
	const size_t TRAIL_LENGTH = 240;
	// How long to keep a projectile around after it hits or expires.
	const int LINGER = 120;
	// The random seed everything starts from.
	const uint64_t SEED = 1;

	bool Same(double a, double b)
	{
		return a == b || (isnan(a) && isnan(b));
	}

	// Whether a script run on a record's input decided what the record says.
	bool Same(const MissileGuidance::Record &record, bool succeeded, const MissileGuidance::Output &output)
	{
		if(succeeded != record.succeeded)
			return false;
		// A script that fails doesn't steer, whatever it set before failing.
		return !succeeded || (Same(output.turn, record.output.turn) && Same(output.thrust, record.output.thrust)
			&& output.dropTarget == record.output.dropTarget);
	}
}



GuidanceSimulation::Missile::Missile(int id, int launchStep, Projectile &&projectile)
	: id(id), launchStep(launchStep), projectile(std::move(projectile))
{
}



GuidanceSimulation::GuidanceSimulation(const Outfit &outfit, const Ship &targetModel)
	: weapon(*outfit.GetWeapon()), targetModel(&targetModel), launcher(targetModel)
{
	auto model = make_shared<Ship>(targetModel);
	const double distance = min(1500., .5 * weapon.Range());
	drills = GuidanceDrill::MakeDrills(distance, model->MaxVelocity());
	Reset();
}



void GuidanceSimulation::SetGuidance(const MissileGuidance *guidance)
{
	weapon.SetGuidance(guidance);
}



const Weapon &GuidanceSimulation::GetWeapon() const
{
	return weapon;
}



const vector<GuidanceDrill> &GuidanceSimulation::Drills() const
{
	return drills;
}



int GuidanceSimulation::Drill() const
{
	return drill;
}



void GuidanceSimulation::SetDrill(int index)
{
	if(index < 0 || index >= static_cast<int>(drills.size()))
		return;
	drill = index;
	Reset();
}



void GuidanceSimulation::Reset()
{
	Random::Seed(SEED);
	missiles.clear();
	step = 0;
	nextId = 0;
	hits = 0;
	records.clear();
	traces.clear();

	target = make_shared<Ship>(*targetModel);
	target->Recharge();
	target->SetGovernment(GameData::Governments().Get("Pirate"));
	launcher = Ship(*targetModel);
	launcher.SetGovernment(GameData::Governments().Get("Republic"));
	launcher.SetTargetShip(target);
	const GuidanceDrill::Motion motion = drills[drill].motion(0);
	target->SetKinematics(motion.position, motion.velocity, motion.facing);
}



void GuidanceSimulation::Step()
{
	++step;
	// As in the game, ships move before projectiles do.
	const GuidanceDrill::Motion motion = drills[drill].motion(step);
	target->SetKinematics(motion.position, motion.velocity, motion.facing);
	if((step - 1) % LAUNCH_INTERVAL == 0)
		Fire();

	vector<Visual> visuals;
	vector<Projectile> spawned;
	for(Missile &missile : missiles)
	{
		if(missile.endStep >= 0)
			continue;
		Projectile &projectile = missile.projectile;
		vector<MissileGuidance::Record> &runs = records[missile.id];
		const size_t runCount = runs.size();
		projectile.Move(visuals, spawned);
		for(size_t i = runCount; i < runs.size(); ++i)
			runs[i].step = step;
		visuals.clear();
		spawned.clear();
		if(projectile.ShouldBeRemoved())
		{
			missile.endStep = step;
			continue;
		}
		missile.trail.push_back(projectile.Position());
		missile.confused.push_back(projectile.IsConfused());
		if(missile.trail.size() > TRAIL_LENGTH)
		{
			missile.trail.pop_front();
			missile.confused.pop_front();
		}
		// Projectiles check for collisions along their next movement.
		Point offset = projectile.Position() - target->Position();
		if(target->GetMask(step).Collide(offset, projectile.Velocity(), target->Facing()) < 1.)
		{
			missile.hit = true;
			missile.endStep = step;
			++hits;
		}
	}
	// Forget projectiles that have been gone for a while.
	missiles.erase(remove_if(missiles.begin(), missiles.end(), [this](const Missile &missile) {
		const bool forget = missile.endStep >= 0 && step - missile.endStep > LINGER && missile.id != selected;
		if(forget)
			traces.erase(missile.id);
		return forget; }), missiles.end());
}



void GuidanceSimulation::ReplayTo(int targetStep)
{
	Reset();
	while(step < targetStep)
		Step();
}



int GuidanceSimulation::CurrentStep() const
{
	return step;
}



const Ship &GuidanceSimulation::Launcher() const
{
	return launcher;
}



const Ship &GuidanceSimulation::Target() const
{
	return *target;
}



const vector<GuidanceSimulation::Missile> &GuidanceSimulation::Missiles() const
{
	return missiles;
}



GuidanceSimulation::Difference GuidanceSimulation::FirstDifference(const MissileGuidance &guidance) const
{
	Difference first;
	for(const auto &it : records)
	{
		// Each projectile's script starts with nothing in memory, and carries
		// its own memory forward from step to step, as it would have.
		script::Memory memory;
		for(const MissileGuidance::Record &record : it.second)
		{
			if(first.step >= 0 && record.step >= first.step)
				break;
			script::RandomTape draws = record.draws;
			draws.Play();
			MissileGuidance::Output output;
			bool succeeded;
			{
				script::RandomTape::Use use(&draws);
				succeeded = guidance.Run(record.input, output, &memory);
			}
			// Drawing other random numbers changes what everything after it gets.
			if(!Same(record, succeeded, output) || !draws.Matches() || !draws.IsFinished())
			{
				first.step = record.step;
				first.missile = it.first;
				first.position = record.input.position;
				break;
			}
		}
	}
	return first;
}



int GuidanceSimulation::Launches() const
{
	return nextId;
}



int GuidanceSimulation::Hits() const
{
	return hits;
}



void GuidanceSimulation::Select(int id)
{
	selected = id;
}



int GuidanceSimulation::Selected() const
{
	return selected;
}



const GuidanceSimulation::Missile *GuidanceSimulation::Followed() const
{
	const Missile *newest = nullptr;
	for(const Missile &missile : missiles)
	{
		if(missile.endStep >= 0)
			continue;
		if(missile.id == selected)
			return &missile;
		if(!newest || missile.id > newest->id)
			newest = &missile;
	}
	return newest;
}



const script::Trace &GuidanceSimulation::TraceOf(int id) const
{
	static const script::Trace NONE;
	auto it = traces.find(id);
	return it == traces.end() ? NONE : it->second;
}



const MissileGuidance::Record *GuidanceSimulation::LastRecord(int id) const
{
	auto it = records.find(id);
	return (it == records.end() || it->second.empty()) ? nullptr : &it->second.back();
}



int GuidanceSimulation::MissileAt(const Point &position, double distance) const
{
	int best = -1;
	double bestDistance = distance;
	for(const Missile &missile : missiles)
	{
		if(missile.endStep >= 0)
			continue;
		double d = missile.projectile.Position().Distance(position);
		if(d <= bestDistance)
		{
			bestDistance = d;
			best = missile.id;
		}
	}
	return best;
}



void GuidanceSimulation::Fire()
{
	missiles.emplace_back(nextId, step, Projectile(launcher, Point(), Angle(), &weapon));
	missiles.back().projectile.SetGuidanceRecords(&records[nextId]);
	missiles.back().projectile.SetGuidanceTrace(&traces[nextId]);
	++nextId;
}
