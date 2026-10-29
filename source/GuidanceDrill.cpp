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

#include <cmath>

using namespace std;

namespace {
	// Motion in a straight line at a constant velocity.
	GuidanceDrill::Motion Straight(Point start, Point velocity, int step)
	{
		return {start + velocity * step, velocity, Angle(velocity ? velocity : Point(0., -1.))};
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
