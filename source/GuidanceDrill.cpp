/* GuidanceDrill.cpp
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

#include "GuidanceDrill.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	using Control = GuidanceDrill::Control;
	using Situation = GuidanceDrill::Situation;

	// Motion in a straight line at a constant velocity.
	GuidanceDrill::Motion Straight(Point start, Point velocity, int step)
	{
		return {start + velocity * step, velocity, Angle(velocity ? velocity : Point(0., -1.))};
	}

	// Turn toward a heading, as fast as the ship can but without overshooting,
	// thrusting once it is facing roughly that way.
	Control Head(const Situation &situation, const Angle &heading, double thrust = 1.)
	{
		const double off = (heading - situation.ship.facing).Degrees();
		Control control;
		control.turn = max(-1., min(1., off / max(.001, situation.turnRate)));
		control.thrust = fabs(off) < 50. ? thrust : .15 * thrust;
		return control;
	}

	// Orbit a point, counterclockwise, wandering in and out.
	Control Cruise(const Situation &situation, const Point &around)
	{
		const double radius = 450. + 200. * sin(situation.step / 260.);
		const Point offset = situation.ship.position - around;
		const Angle bearing = offset ? Angle(offset) : Angle();
		// Aim for a point a little way around the orbit.
		const Point aim = around + (bearing - Angle(55.)).Unit() * radius;
		return Head(situation, Angle(aim - situation.ship.position));
	}

	// The nearest missile closing in, within the given distance, if any.
	const GuidanceDrill::Motion *Threat(const Situation &situation, double within)
	{
		const GuidanceDrill::Motion *threat = nullptr;
		double nearest = within;
		for(const GuidanceDrill::Motion &missile : situation.missiles)
		{
			const Point offset = situation.ship.position - missile.position;
			const Point closing = missile.velocity - situation.ship.velocity;
			const double distance = offset.Length();
			if(distance < nearest && offset.Dot(closing) > 0.)
			{
				nearest = distance;
				threat = &missile;
			}
		}
		return threat;
	}
}



vector<GuidanceDrill> GuidanceDrill::MakeDrills(double distance, double speed)
{
	const Point ahead(0., -distance);
	vector<GuidanceDrill> drills;
	drills.push_back({"ahead, holding still", [=](int step) {
		return Straight(ahead, Point(), step); }});
	drills.push_back({"ahead, fleeing", [=](int step) {
		return Straight(ahead, Point(0., -speed), step); }});
	drills.push_back({"ahead, approaching", [=](int step) {
		return Straight(ahead, Point(0., speed), step); }});
	drills.push_back({"ahead, crossing", [=](int step) {
		return Straight(ahead, Point(speed, 0.), step); }});
	drills.push_back({"ahead, jinking", [=](int step) {
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
	drills.push_back({"ahead, circling", [=](int step) {
		// Circle around the starting point, counterclockwise.
		const double radius = 300.;
		double theta = speed * step / radius;
		Point offset(radius * sin(theta), -radius * cos(theta));
		Point velocity(speed * cos(theta), speed * sin(theta));
		return Motion{ahead + offset - Point(0., -radius), velocity, Angle(velocity)}; }});
	drills.push_back({"to the side, holding still", [=](int step) {
		return Straight(Point(distance, 0.), Point(), step); }});
	drills.push_back({"behind, holding still", [=](int step) {
		return Straight(Point(0., distance), Point(), step); }});
	return drills;
}



vector<GuidanceDrill> GuidanceDrill::MakeFlownDrills(double distance)
{
	const Point ahead(0., -distance);
	vector<GuidanceDrill> drills;
	// Each starts ahead, flying across the launcher's view.
	const Motion start{ahead, Point(), Angle(90.)};

	GuidanceDrill evading {"evading", nullptr, start, [ahead](const Situation &situation) {
		// Cruise, until a missile closes in: then turn to cross its path, on
		// whichever side the ship is already on, and burn.
		const Motion *threat = Threat(situation, 1000.);
		if(!threat || !threat->velocity)
			return Cruise(situation, ahead);
		const Point across(-threat->velocity.Y(), threat->velocity.X());
		const bool left = across.Dot(situation.ship.position - threat->position) > 0.;
		return Head(situation, Angle(left ? across : -across)); }};
	drills.push_back(evading);

	GuidanceDrill cruising {"cruising", nullptr, start, [ahead](const Situation &situation) {
		return Cruise(situation, ahead); }};
	drills.push_back(cruising);

	GuidanceDrill breaking {"breaking away", nullptr, start, [](const Situation &situation) {
		// Run from the launcher, weaving from side to side.
		const Point away = situation.ship.position - situation.launcher;
		const Angle weave(35. * sin(situation.step / 70.));
		return Head(situation, Angle(away ? away : Point(0., -1.)) + weave); }};
	drills.push_back(breaking);
	return drills;
}
