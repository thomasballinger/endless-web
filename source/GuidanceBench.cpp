/* GuidanceBench.cpp
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

#include "GuidanceBench.h"

#include "Angle.h"
#include "GameData.h"
#include "Government.h"
#include "image/Mask.h"
#include "MissileGuidance.h"
#include "Outfit.h"
#include "pi.h"
#include "Point.h"
#include "Projectile.h"
#include "Random.h"
#include "Ship.h"
#include "Visual.h"
#include "Weapon.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

using namespace std;

namespace {
	// Where the target is on a given step.
	class Motion {
	public:
		Point position;
		Point velocity;
		Angle facing;
	};

	class Scenario {
	public:
		string name;
		// The target's motion on the given step (step 0 is when the projectile is fired).
		function<Motion(int step)> motion;
	};

	// Motion in a straight line at a constant velocity.
	Motion Straight(Point start, Point velocity, int step)
	{
		return {start + velocity * step, velocity, Angle(velocity ? velocity : Point(0., -1.))};
	}

	// The scenarios, for a projectile fired from the origin facing "up"
	// (negative y), at a target that starts the given distance away and moves
	// at the given speed.
	vector<Scenario> MakeScenarios(double distance, double speed)
	{
		const Point ahead(0., -distance);
		vector<Scenario> scenarios;
		scenarios.push_back({"ahead, holding still", [=](int step) {
			return Straight(ahead, Point(), step); }});
		scenarios.push_back({"ahead, fleeing", [=](int step) {
			return Straight(ahead, Point(0., -speed), step); }});
		scenarios.push_back({"ahead, approaching", [=](int step) {
			return Straight(ahead, Point(0., speed), step); }});
		scenarios.push_back({"ahead, crossing", [=](int step) {
			return Straight(ahead, Point(speed, 0.), step); }});
		scenarios.push_back({"ahead, jinking", [=](int step) {
			// Cross back and forth, turning around every second.
			Point position = ahead;
			Point velocity;
			for(int i = 0; i <= step; ++i)
			{
				velocity = Point((i / 60) % 2 ? -speed : speed, 0.);
				if(i < step)
					position += velocity;
			}
			return Motion{position, velocity, Angle(velocity)}; }});
		scenarios.push_back({"ahead, circling", [=](int step) {
			// Circle around the starting point, counterclockwise.
			const double radius = 300.;
			double theta = speed * step / radius;
			Point offset(radius * sin(theta), -radius * cos(theta));
			Point velocity(speed * cos(theta), speed * sin(theta));
			return Motion{ahead + offset - Point(0., -radius), velocity, Angle(velocity)}; }});
		scenarios.push_back({"to the side, holding still", [=](int step) {
			return Straight(Point(distance, 0.), Point(), step); }});
		scenarios.push_back({"behind, holding still", [=](int step) {
			return Straight(Point(0., distance), Point(), step); }});
		return scenarios;
	}

	// One shot at the target.
	class Shot {
	public:
		bool hit = false;
		int steps = 0;
		double closest = numeric_limits<double>::infinity();
		vector<Point> projectilePath;
		vector<Point> targetPath;
	};

	Shot Fire(const Weapon &weapon, Ship &launcher, const shared_ptr<Ship> &target, const Scenario &scenario,
		uint64_t seed)
	{
		Random::Seed(seed);
		Shot shot;
		Motion motion = scenario.motion(0);
		target->SetKinematics(motion.position, motion.velocity, motion.facing);

		vector<Visual> visuals;
		vector<Projectile> spawned;
		Projectile projectile(launcher, Point(), Angle(), &weapon);
		shot.projectilePath.push_back(projectile.Position());
		shot.targetPath.push_back(target->Position());
		for(int step = 1; ; ++step)
		{
			// As in the game, ships move before projectiles do, and then
			// projectiles check for collisions along their next movement.
			motion = scenario.motion(step);
			target->SetKinematics(motion.position, motion.velocity, motion.facing);
			projectile.Move(visuals, spawned);
			visuals.clear();
			spawned.clear();
			if(projectile.ShouldBeRemoved())
				break;
			shot.steps = step;
			shot.projectilePath.push_back(projectile.Position());
			shot.targetPath.push_back(target->Position());

			Point offset = projectile.Position() - target->Position();
			shot.closest = min(shot.closest, offset.Length());
			if(target->GetMask(step).Collide(offset, projectile.Velocity(), target->Facing()) < 1.)
			{
				shot.hit = true;
				break;
			}
		}
		return shot;
	}

	void WritePath(ostream &out, const vector<Point> &path)
	{
		out << '[';
		for(size_t i = 0; i < path.size(); ++i)
		{
			if(i)
				out << ',';
			out << lround(path[i].X()) << ',' << lround(path[i].Y());
		}
		out << ']';
	}

	// Quote a string for JSON (names in game data have no control characters).
	string Quote(const string &text)
	{
		string result = "\"";
		for(char c : text)
		{
			if(c == '"' || c == '\\')
				result += '\\';
			result += c;
		}
		return result + '"';
	}
}



bool GuidanceBench::Run(const Options &options)
{
	const Outfit *outfit = GameData::Outfits().Find(options.outfit);
	if(!outfit || !outfit->GetWeapon() || !outfit->GetWeapon()->Homing())
	{
		fprintf(stderr, "\"%s\" is not a homing weapon.\n", options.outfit.c_str());
		return false;
	}
	Weapon weapon = *outfit->GetWeapon();
	if(!options.guidance.empty())
	{
		const MissileGuidance *guidance = GameData::Guidance().Find(options.guidance);
		if(!guidance || !guidance->IsValid())
		{
			fprintf(stderr, "There is no valid guidance script named \"%s\".\n", options.guidance.c_str());
			return false;
		}
		string missing = guidance->MissingCapability(weapon);
		if(!missing.empty())
		{
			fprintf(stderr, "\"%s\" can't use guidance \"%s\" because %s.\n", options.outfit.c_str(),
				options.guidance.c_str(), missing.c_str());
			return false;
		}
		weapon.SetGuidance(guidance);
	}
	const MissileGuidance *guidance = weapon.Guidance() ? weapon.Guidance() : MissileGuidance::Default(weapon);

	const Ship *model = GameData::Ships().Find(options.target);
	if(!model)
	{
		fprintf(stderr, "There is no ship model named \"%s\".\n", options.target.c_str());
		return false;
	}
	auto target = make_shared<Ship>(*model);
	target->Recharge();
	target->SetGovernment(GameData::Governments().Get("Pirate"));
	Ship launcher(*model);
	launcher.SetGovernment(GameData::Governments().Get("Republic"));
	launcher.SetTargetShip(target);

	const double speed = target->MaxVelocity();
	const double distance = min(1500., .5 * weapon.Range());
	printf("Guidance bench: %s, guidance \"%s\"\n", options.outfit.c_str(),
		guidance ? guidance->Name().c_str() : "(none)");
	printf("Target: %s, moving at %.1f, starting %.0f away. %d shots per scenario.\n\n",
		options.target.c_str(), speed, distance, options.trials);
	printf("%-28s %7s %12s %14s\n", "scenario", "hits", "mean steps", "closest miss");

	ofstream replay;
	if(!options.replayPath.empty())
	{
		replay.open(options.replayPath);
		replay << "{\"outfit\":" << Quote(options.outfit) << ",\"guidance\":"
			<< Quote(guidance ? guidance->Name() : "") << ",\"target\":" << Quote(options.target)
			<< ",\"targetRadius\":" << lround(target->Radius()) << ",\"scenarios\":[";
	}

	int totalHits = 0;
	int totalShots = 0;
	const vector<Scenario> scenarios = MakeScenarios(distance, speed);
	for(size_t s = 0; s < scenarios.size(); ++s)
	{
		int hits = 0;
		double hitSteps = 0.;
		double closestMiss = numeric_limits<double>::infinity();
		if(replay.is_open())
			replay << (s ? "," : "") << "{\"name\":" << Quote(scenarios[s].name) << ",\"shots\":[";
		for(int trial = 0; trial < options.trials; ++trial)
		{
			Shot shot = Fire(weapon, launcher, target, scenarios[s], 1000 * s + trial);
			if(shot.hit)
			{
				++hits;
				hitSteps += shot.steps;
			}
			else
				closestMiss = min(closestMiss, shot.closest);
			if(replay.is_open())
			{
				replay << (trial ? "," : "") << "{\"hit\":" << (shot.hit ? "true" : "false")
					<< ",\"steps\":" << shot.steps << ",\"projectile\":";
				WritePath(replay, shot.projectilePath);
				replay << ",\"target\":";
				WritePath(replay, shot.targetPath);
				replay << '}';
			}
		}
		if(replay.is_open())
			replay << "]}";
		totalHits += hits;
		totalShots += options.trials;

		string hitText = to_string(hits) + "/" + to_string(options.trials);
		string stepText = hits ? to_string(lround(hitSteps / hits)) : "-";
		string missText = isinf(closestMiss) ? "-" : to_string(lround(closestMiss));
		printf("%-28s %7s %12s %14s\n", scenarios[s].name.c_str(), hitText.c_str(), stepText.c_str(),
			missText.c_str());
	}
	if(replay.is_open())
		replay << "]}\n";
	printf("\nTotal: %d of %d shots hit.\n", totalHits, totalShots);
	return true;
}
