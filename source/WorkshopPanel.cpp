/* WorkshopPanel.cpp
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

#include "WorkshopPanel.h"

#include "Color.h"
#include "Command.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "MissileGuidance.h"
#include "MouseButton.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "Screen.h"
#include "Ship.h"
#include "SimulatorPanel.h"
#include "UI.h"
#include "Weapon.h"

#include <algorithm>
#include <map>

using namespace std;

namespace {
	const double WIDTH = 720.;
	const double ROW_HEIGHT = 46.;

	const Color BACKGROUND(.01f, .04f, .025f, .97f);
	const Color BORDER(.25f, .65f, .40f, 1.f);
	const Color TITLE(.75f, .95f, .80f, 1.f);
	const Color TEXT(.60f, .80f, .65f, 1.f);
	const Color DIM(.40f, .55f, .45f, 1.f);
	const Color PROGRAMMED(.55f, .80f, 1.f, 1.f);
	const Color ROW(.03f, .08f, .05f, 1.f);

	// The ship model the simulator fires at first.
	const string TARGET = "Argosy";
}



WorkshopPanel::WorkshopPanel(PlayerInfo &player)
	: player(player)
{
	SetInterruptible(false);
	FindWeapons();
}



void WorkshopPanel::Draw()
{
	DrawBackdrop();
	// Weapons may have been programmed since this was last drawn.
	FindWeapons();

	const Font &font = FontSet::Get(14);
	const Font &bigFont = FontSet::Get(18);
	const double height = 150. + max<size_t>(1, rows.size()) * ROW_HEIGHT;
	const Rectangle box(Point(), Point(WIDTH, height));
	FillShader::Fill(box.Center(), box.Dimensions(), BACKGROUND);
	FillShader::Fill(Point(box.Center().X(), box.Top() + 1.), Point(WIDTH, 2.), BORDER);

	double y = box.Top() + 20.;
	bigFont.Draw("Missile Workshop", Point(box.Left() + 24., y), TITLE);
	closeBox = Rectangle::WithCorners(Point(box.Right() - 40., box.Top() + 10.),
		Point(box.Right() - 10., box.Top() + 40.));
	font.Draw("x", closeBox.Center() - Point(font.Width("x") / 2., 8.), DIM);
	y += 30.;
	font.Draw("Every guided missile runs a program that decides how it steers. Choose a weapon to",
		Point(box.Left() + 24., y), TEXT);
	y += 18.;
	font.Draw("write and test its program; installing it changes how all your ships' missiles fly.",
		Point(box.Left() + 24., y), TEXT);
	y += 34.;

	if(rows.empty())
	{
		font.Draw("None of your ships carries a guided weapon. Missile launchers are sold at outfitters.",
			Point(box.Left() + 24., y + 12.), DIM);
		return;
	}
	for(Row &row : rows)
	{
		row.box = Rectangle::WithCorners(Point(box.Left() + 16., y), Point(box.Right() - 16., y + ROW_HEIGHT - 6.));
		const bool hover = row.box.Contains(UI::GetMouse());
		FillShader::Fill(row.box.Center(), row.box.Dimensions(), hover ? Color(.06f, .14f, .09f, 1.f) : ROW);
		const Weapon &weapon = *row.outfit->GetWeapon();
		const bool programmed = player.GuidancePrograms().contains(row.outfit);
		font.Draw(row.outfit->DisplayName() + (row.count > 1 ? "  x" + to_string(row.count) : ""),
			row.box.TopLeft() + Point(10., 4.), TITLE);
		string hardware = "hardware:";
		hardware += weapon.Leading() ? " leading" : "";
		hardware += weapon.ThrottleControl() ? " throttle" : "";
		hardware += weapon.OpticalTracking() ? " optical" : "";
		hardware += weapon.HasBlindspot() ? " blindspot" : "";
		if(hardware == "hardware:")
			hardware += " basic seeker";
		font.Draw(hardware, row.box.TopLeft() + Point(10., 21.), DIM);
		const MissileGuidance *factory = MissileGuidance::Default(weapon);
		const string status = programmed ? "your program" : "factory: " + (factory ? factory->Name() : string("none"));
		font.Draw(status, Point(row.box.Right() - 10. - font.Width(status), row.box.Top() + 12.),
			programmed ? PROGRAMMED : DIM);
		y += ROW_HEIGHT;
	}
}



bool WorkshopPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(key == SDLK_ESCAPE || key == 'w' || command.Has(Command::MENU))
		GetUI().Pop(this);
	else if(key >= '1' && key <= '9' && static_cast<size_t>(key - '1') < rows.size())
		Open(*rows[key - '1'].outfit);
	else
		return false;
	return true;
}



bool WorkshopPanel::Click(int x, int y, MouseButton button, int clicks)
{
	if(button != MouseButton::LEFT)
		return false;
	const Point point(x, y);
	if(closeBox.Contains(point))
	{
		GetUI().Pop(this);
		return true;
	}
	for(const Row &row : rows)
		if(row.box.Contains(point))
		{
			Open(*row.outfit);
			return true;
		}
	return true;
}



void WorkshopPanel::FindWeapons()
{
	// Every kind of homing weapon on the player's ships, in the order the
	// fleet lists them.
	vector<Row> found;
	for(const shared_ptr<Ship> &ship : player.Ships())
	{
		if(ship->IsDestroyed() || ship->IsParked())
			continue;
		for(const auto &[outfit, count] : ship->Outfits())
		{
			if(!outfit->GetWeapon() || !outfit->GetWeapon()->Homing())
				continue;
			auto it = find_if(found.begin(), found.end(), [outfit](const Row &row) { return row.outfit == outfit; });
			if(it == found.end())
				found.push_back({outfit, count, Rectangle()});
			else
				it->count += count;
		}
	}
	// Keep where the rows were drawn, for clicks.
	for(Row &row : found)
		for(const Row &old : rows)
			if(old.outfit == row.outfit)
				row.box = old.box;
	rows = std::move(found);
}



void WorkshopPanel::Open(const Outfit &outfit)
{
	const Ship *target = GameData::Ships().Find(TARGET);
	if(target)
		GetUI().Push(new SimulatorPanel(outfit, *target, &player));
}
