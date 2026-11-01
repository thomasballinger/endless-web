/* DefenseDaemon.cpp
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

#include "DefenseDaemon.h"

#include "DataFile.h"
#include "DataNode.h"
#include "GameData.h"
#include "Projectile.h"
#include "Ship.h"
#include "Weapon.h"

#include <cmath>
#include <limits>
#include <sstream>

using namespace std;

namespace {
	script::Interface MakeInterface()
	{
		script::Interface interface;
		// These must be added in the same order as DefenseDaemon::Variable.
		interface.AddInput("position");
		interface.AddInput("velocity");
		interface.AddInput("shields");
		interface.AddInput("hull");
		interface.AddInput("missile-position");
		interface.AddInput("missile-velocity");
		interface.AddInput("missile-offset");
		interface.AddInput("missile-distance");
		interface.AddInput("closing-speed");
		interface.AddInput("steps-away");
		interface.AddInput("shield-damage");
		interface.AddInput("hull-damage");
		interface.AddInput("steps-left");
		interface.AddInput("locked");
		interface.AddInput("confused");
		interface.AddInput("hacked");
		interface.AddInput("chasing-you");
		interface.AddOutput("score");
		return interface;
	}

	unique_ptr<DefenseDaemon> &PlayerDaemon()
	{
		static unique_ptr<DefenseDaemon> daemon;
		return daemon;
	}
}



const script::Interface &DefenseDaemon::GetInterface()
{
	static const script::Interface interface = MakeInterface();
	return interface;
}



DefenseDaemon::Input DefenseDaemon::MakeInput(const Ship &ship, const Projectile &missile)
{
	Input input;
	input.position = ship.Position();
	input.velocity = ship.Velocity();
	input.shields = ship.ShieldFraction();
	input.hull = ship.HullFraction();
	input.missilePosition = missile.Position();
	input.missileVelocity = missile.Velocity();
	const Weapon &weapon = missile.GetWeapon();
	input.shieldDamage = weapon.ShieldDamage();
	input.hullDamage = weapon.HullDamage();
	input.stepsLeft = missile.StepsLeft();
	input.locked = missile.HasLock();
	input.confused = missile.IsConfused();
	input.hacked = missile.HackedSteps();
	return input;
}



void DefenseDaemon::Load(const DataNode &node, bool logErrors)
{
	program = script::Program();
	program.Load(node, GetInterface(), logErrors);
}



const string &DefenseDaemon::Name() const
{
	return program.Name();
}



bool DefenseDaemon::IsValid() const
{
	return program.IsValid();
}



const script::Program &DefenseDaemon::Program() const
{
	return program;
}



bool DefenseDaemon::Score(const Input &input, double &score, script::Trace *trace) const
{
	thread_local vector<script::Value> variables;
	variables.resize(program.SlotCount());
	variables[POSITION] = script::Value(input.position);
	variables[VELOCITY] = script::Value(input.velocity);
	variables[SHIELDS] = script::Value(input.shields);
	variables[HULL] = script::Value(input.hull);
	variables[MISSILE_POSITION] = script::Value(input.missilePosition);
	variables[MISSILE_VELOCITY] = script::Value(input.missileVelocity);
	const Point offset = input.missilePosition - input.position;
	const double distance = offset.Length();
	variables[MISSILE_OFFSET] = script::Value(offset);
	variables[MISSILE_DISTANCE] = script::Value(distance);
	// How fast the missile is coming closer: its velocity relative to the
	// ship, along the line from it to the ship.
	const double closing = distance ? -(input.missileVelocity - input.velocity).Dot(offset) / distance : 0.;
	variables[CLOSING_SPEED] = script::Value(closing);
	variables[STEPS_AWAY] = script::Value(closing > 0. ? distance / closing : 1e9);
	variables[SHIELD_DAMAGE] = script::Value(input.shieldDamage);
	variables[HULL_DAMAGE] = script::Value(input.hullDamage);
	variables[STEPS_LEFT] = script::Value(static_cast<double>(input.stepsLeft));
	variables[LOCKED] = script::Value(input.locked);
	variables[CONFUSED] = script::Value(input.confused);
	variables[HACKED] = script::Value(static_cast<double>(input.hacked));
	variables[CHASING_YOU] = script::Value(input.chasingYou);
	variables[SCORE] = script::Value(0.);

	uint32_t effects = 0;
	if(!program.Run(variables, effects, nullptr, trace ? trace : script::Activity::Begin("daemon", Name()))
			|| variables[SCORE].GetType() != script::Value::Type::NUMBER)
		return false;
	score = variables[SCORE].Number();
	return true;
}



pair<const Ship *, Projectile *> DefenseDaemon::Choose(const vector<pair<const Ship *, Projectile *>> &missiles,
	const Ship *runner) const
{
	pair<const Ship *, Projectile *> chosen(nullptr, nullptr);
	double best = 0.;
	for(const auto &[ship, missile] : missiles)
	{
		Input input = MakeInput(*ship, *missile);
		input.chasingYou = (ship == runner);
		double score = 0.;
		if(Score(input, score) && score > best)
		{
			best = score;
			chosen = {ship, missile};
		}
	}
	return chosen;
}



const DefenseDaemon *DefenseDaemon::Factory()
{
	const DefenseDaemon *daemon = GameData::Daemons().Find("nearest first");
	return daemon && daemon->IsValid() ? daemon : nullptr;
}



bool DefenseDaemon::SetPlayerProgram(const string &text)
{
	PlayerDaemon().reset();
	if(text.empty())
		return false;
	istringstream in(text);
	DataFile file(in);
	for(const DataNode &node : file)
		if(node.Token(0) == "daemon" && node.Size() >= 2)
		{
			auto daemon = make_unique<DefenseDaemon>();
			daemon->Load(node, false);
			if(!daemon->IsValid())
				return false;
			PlayerDaemon() = std::move(daemon);
			return true;
		}
	return false;
}



const DefenseDaemon *DefenseDaemon::PlayerProgram()
{
	return PlayerDaemon() ? PlayerDaemon().get() : Factory();
}
