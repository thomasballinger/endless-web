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
#include "DataFile.h"
#include "DataNode.h"
#include "DefenseDaemon.h"
#include "GameData.h"
#include "Government.h"
#include "GuidanceDrill.h"
#include "GuidanceSimulation.h"
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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace {
	using Motion = GuidanceDrill::Motion;
	using Scenario = GuidanceDrill;

	vector<Scenario> MakeScenarios(double distance, double speed)
	{
		return GuidanceDrill::MakeDrills(distance, speed);
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
	// What one guidance script did in one scenario.
	class Result {
	public:
		int hits = 0;
		double hitSteps = 0.;
		double closestMiss = numeric_limits<double>::infinity();
	};

	// What can be seen of one projectile in the simulator.
	class Seen {
	public:
		explicit Seen(const GuidanceSimulation::Missile &missile)
			: id(missile.id), position(missile.projectile.Position()), velocity(missile.projectile.Velocity()),
			facing(missile.projectile.Facing().Degrees()), endStep(missile.endStep), hit(missile.hit) {}

		bool operator==(const Seen &other) const = default;

		int id;
		Point position;
		Point velocity;
		double facing;
		int endStep;
		bool hit;
	};

	vector<Seen> See(const GuidanceSimulation &simulation)
	{
		vector<Seen> seen;
		for(const GuidanceSimulation::Missile &missile : simulation.Missiles())
			seen.emplace_back(missile);
		return seen;
	}

	// For each drill, run the simulator with the weapon's own script, and then
	// with each other script: the first step on which anything differs must be
	// the step FirstDifference() found, or later if the change took a while to
	// show (e.g. dropping the target), and never earlier.
	bool CheckEdits(const Outfit &outfit, const Ship &model, const vector<Weapon> &weapons,
		const vector<string> &labels)
	{
		const int STEPS = 1200;
		GuidanceSimulation original(outfit, model);
		GuidanceSimulation edited(outfit, model);
		original.SetGuidance(weapons[0].Guidance());
		printf("Edit check: %s, %d steps of each drill, against \"%s\"\n\n", outfit.TrueName().c_str(), STEPS,
			labels[0].c_str());
		printf("%-28s %-30s %10s %10s\n", "drill", "script", "mattered", "differed");
		bool ok = true;
		for(size_t d = 0; d < original.Drills().size(); ++d)
		{
			original.SetDrill(d);
			vector<vector<Seen>> seen(1, See(original));
			while(original.CurrentStep() < STEPS)
			{
				original.Step();
				seen.push_back(See(original));
				// Between steps the game may draw random numbers of its own (in the
				// simulator, between frames), which must not change anything.
				Random::Real();
			}
			// Going back to a step, and stepping on from there, must show exactly
			// what running straight through did.
			for(int step : {1, 2, 89, 90, 91, 300, 517, 600, 901, STEPS - 1})
			{
				original.ReplayTo(step);
				bool same = (See(original) == seen[step]);
				original.Step();
				same &= (See(original) == seen[step + 1]);
				if(!same)
				{
					printf("%-28s replaying to step %d is NOT exact\n", original.Drills()[d].name.c_str(), step);
					ok = false;
				}
			}
			for(size_t w = 1; w < weapons.size(); ++w)
			{
				const MissileGuidance *guidance = weapons[w].Guidance();
				const int mattered = original.FirstDifference(*guidance).step;
				edited.SetGuidance(guidance);
				edited.SetDrill(d);
				int differed = -1;
				while(differed < 0 && edited.CurrentStep() < STEPS)
				{
					edited.Step();
					if(See(edited) != seen[edited.CurrentStep()])
						differed = edited.CurrentStep();
				}
				const bool right = mattered < 0 ? differed < 0 : (differed < 0 || differed >= mattered);
				ok &= right;
				auto text = [](int step) { return step < 0 ? string("never") : to_string(step); };
				printf("%-28s %-30s %10s %10s%s\n", original.Drills()[d].name.c_str(), labels[w].c_str(),
					text(mattered).c_str(), text(differed).c_str(), right ? "" : "  WRONG");
			}
		}
		printf("\n%s\n", ok ? "Every edit mattered first where the simulator said."
			: "The simulator was WRONG about where some edits first mattered.");
		return ok;
	}

	// Load every "guidance" node in the given files, each under its first
	// name only, so that they don't replace the game's own scripts. Their names
	// are added to the given list, in order, and the file each came from to the
	// given map.
	bool LoadScriptFiles(const vector<string> &paths, map<string, MissileGuidance> &scripts, vector<string> &names,
		map<string, string> &sources)
	{
		for(const string &path : paths)
		{
			if(!filesystem::is_regular_file(path))
			{
				fprintf(stderr, "Can't read the script file \"%s\".\n", path.c_str());
				return false;
			}
			size_t count = 0;
			for(const DataNode &node : DataFile(path))
				if(node.Token(0) == "guidance" && node.Size() >= 2)
				{
					const string &name = node.Token(1);
					if(!scripts.contains(name))
						names.push_back(name);
					scripts[name].Load(node, name);
					sources[name] = path;
					++count;
				}
			if(!count)
				fprintf(stderr, "Warning: \"%s\" has no guidance scripts in it.\n", path.c_str());
		}
		return true;
	}

	// Find the guidance script with the given name, preferring scripts from files.
	const MissileGuidance *FindGuidance(const string &name, const map<string, MissileGuidance> &scripts)
	{
		auto it = scripts.find(name);
		return it != scripts.end() ? &it->second : GameData::Guidance().Find(name);
	}

	// The replay file for one of several guidance scripts: the given path with
	// the script's name added before the extension.
	string ReplayPath(const string &path, const string &name, bool several)
	{
		if(!several)
			return path;
		string slug;
		for(char c : name)
			slug += isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(tolower(c)) : '-';
		filesystem::path result(path);
		return (result.parent_path() / (result.stem().string() + "-" + slug + result.extension().string())).string();
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
	if(!options.daemons.empty())
	{
		// For each drill: how many missiles hit a target that runs no daemon,
		// and one that runs each of these.
		const Ship *model = GameData::Ships().Find(options.target);
		if(!model)
			return false;
		const int STEPS = 3000;
		printf("Defense daemons: %s at %s, %d steps of each drill\n\n", outfit->TrueName().c_str(),
			options.target.c_str(), STEPS);
		printf("%-28s %-20s %8s %8s\n", "drill", "daemon", "fired", "hit");
		GuidanceSimulation simulation(*outfit, *model);
		vector<string> names = {""};
		names.insert(names.end(), options.daemons.begin(), options.daemons.end());
		for(size_t drill = 0; drill < simulation.Drills().size(); ++drill)
			for(const string &name : names)
			{
				const DefenseDaemon *daemon = name.empty() ? nullptr : GameData::Daemons().Find(name);
				if(!name.empty() && (!daemon || !daemon->IsValid()))
				{
					fprintf(stderr, "No valid daemon \"%s\".\n", name.c_str());
					return false;
				}
				simulation.SetTargetDaemon(daemon);
				simulation.SetDrill(drill);
				simulation.ReplayTo(STEPS);
				printf("%-28s %-20s %8d %8d\n", simulation.Drills()[drill].name.c_str(),
					name.empty() ? "(none)" : name.c_str(), simulation.Launches(), simulation.Hits());
			}
		return true;
	}
	Weapon original = *outfit->GetWeapon();
	if(options.seekerNoise >= 0.)
		original.SetSeekerNoise(options.seekerNoise);

	// Scripts loaded from files must outlive the weapons that point to them.
	map<string, MissileGuidance> fileScripts;
	vector<string> fileNames;
	map<string, string> sources;
	if(!LoadScriptFiles(options.scriptFiles, fileScripts, fileNames, sources))
		return false;

	// Which guidance scripts to compare: the ones named, or else the weapon's
	// own followed by every script from the files.
	vector<string> names = options.guidance;
	if(names.empty())
	{
		names.push_back("");
		names.insert(names.end(), fileNames.begin(), fileNames.end());
	}
	vector<Weapon> weapons;
	vector<string> labels;
	for(const string &name : names)
	{
		Weapon weapon = original;
		if(!name.empty())
		{
			const MissileGuidance *guidance = FindGuidance(name, fileScripts);
			if(!guidance || !guidance->IsValid())
			{
				fprintf(stderr, "There is no valid guidance script named \"%s\".\n", name.c_str());
				return false;
			}
			string missing = guidance->MissingCapability(weapon);
			if(!missing.empty())
			{
				fprintf(stderr, "\"%s\" can't use guidance \"%s\" because %s.\n", options.outfit.c_str(),
					name.c_str(), missing.c_str());
				return false;
			}
			weapon.SetGuidance(guidance);
		}
		const MissileGuidance *guidance = weapon.Guidance() ? weapon.Guidance() : MissileGuidance::Default(weapon);
		labels.push_back(guidance ? guidance->Name() : "(none)");
		if(name.empty())
			sources[labels.back()] = "the weapon's own";
		else if(!sources.contains(name))
			sources[name] = "game data";
		weapons.push_back(weapon);
	}

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

	if(options.editCheck)
		return CheckEdits(*outfit, *model, weapons, labels);

	const double speed = target->MaxVelocity();
	const double distance = min(1500., .5 * original.Range());
	const vector<Scenario> scenarios = MakeScenarios(distance, speed);
	const bool several = weapons.size() > 1;

	// Fire every weapon in every scenario. Each shot in a scenario starts from
	// the same random seed whichever script steers it.
	vector<vector<Result>> results(weapons.size(), vector<Result>(scenarios.size()));
	for(size_t w = 0; w < weapons.size(); ++w)
	{
		ofstream replay;
		if(!options.replayPath.empty())
		{
			string path = ReplayPath(options.replayPath, labels[w], several);
			replay.open(path);
			if(several)
				printf("Writing the paths of the \"%s\" shots to %s\n", labels[w].c_str(), path.c_str());
			replay << "{\"outfit\":" << Quote(options.outfit) << ",\"guidance\":"
				<< Quote(labels[w] == "(none)" ? "" : labels[w]) << ",\"target\":" << Quote(options.target)
				<< ",\"targetRadius\":" << lround(target->Radius()) << ",\"scenarios\":[";
		}
		for(size_t s = 0; s < scenarios.size(); ++s)
		{
			Result &result = results[w][s];
			if(replay.is_open())
				replay << (s ? "," : "") << "{\"name\":" << Quote(scenarios[s].name) << ",\"shots\":[";
			for(int trial = 0; trial < options.trials; ++trial)
			{
				Shot shot = Fire(weapons[w], launcher, target, scenarios[s], 1000 * s + trial);
				if(shot.hit)
				{
					++result.hits;
					result.hitSteps += shot.steps;
				}
				else
					result.closestMiss = min(result.closestMiss, shot.closest);
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
		}
		if(replay.is_open())
			replay << "]}\n";
	}
	if(several)
		printf("\n");

	auto hitText = [&options](const Result &result) {
		return to_string(result.hits) + "/" + to_string(options.trials);
	};
	auto stepText = [](const Result &result) {
		return result.hits ? to_string(lround(result.hitSteps / result.hits)) : string("-");
	};

	if(!several)
	{
		printf("Guidance bench: %s, guidance \"%s\"\n", options.outfit.c_str(), labels[0].c_str());
		if(original.SeekerNoise())
			printf("Seeker noise: %g degrees.\n", original.SeekerNoise());
		printf("Target: %s, moving at %.1f, starting %.0f away. %d shots per scenario.\n\n",
			options.target.c_str(), speed, distance, options.trials);
		printf("%-28s %7s %12s %14s\n", "scenario", "hits", "mean steps", "closest miss");
		int totalHits = 0;
		for(size_t s = 0; s < scenarios.size(); ++s)
		{
			const Result &result = results[0][s];
			totalHits += result.hits;
			string missText = isinf(result.closestMiss) ? "-" : to_string(lround(result.closestMiss));
			printf("%-28s %7s %12s %14s\n", scenarios[s].name.c_str(), hitText(result).c_str(),
				stepText(result).c_str(), missText.c_str());
		}
		printf("\nTotal: %d of %zu shots hit.\n", totalHits, options.trials * scenarios.size());
		return true;
	}

	// Several scripts: one column of hits and mean steps to impact for each.
	printf("Guidance bench: %s, comparing %zu guidance scripts\n", options.outfit.c_str(), weapons.size());
	if(original.SeekerNoise())
		printf("Seeker noise: %g degrees.\n", original.SeekerNoise());
	printf("Target: %s, moving at %.1f, starting %.0f away. %d shots per scenario.\n",
		options.target.c_str(), speed, distance, options.trials);
	printf("Each column shows hits, and the mean steps to impact, for one script:\n");
	for(const string &label : labels)
		printf("  %-30s %s\n", ("\"" + label + "\"").c_str(), sources[label].c_str());
	printf("\n");
	vector<int> widths;
	printf("%-28s", "scenario");
	for(const string &label : labels)
	{
		widths.push_back(max<int>(14, label.size()));
		printf("  %*s", widths.back(), label.c_str());
	}
	printf("\n");
	vector<int> totals(weapons.size());
	for(size_t s = 0; s < scenarios.size(); ++s)
	{
		printf("%-28s", scenarios[s].name.c_str());
		for(size_t w = 0; w < weapons.size(); ++w)
		{
			const Result &result = results[w][s];
			totals[w] += result.hits;
			string text = hitText(result) + " " + stepText(result);
			printf("  %*s", widths[w], text.c_str());
		}
		printf("\n");
	}
	printf("%-28s", "total hits");
	for(size_t w = 0; w < weapons.size(); ++w)
	{
		string text = to_string(totals[w]) + "/" + to_string(options.trials * scenarios.size());
		printf("  %*s", widths[w], text.c_str());
	}
	printf("\n");
	return true;
}
