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
#include "DefenseDaemon.h"
#include "EscortRole.h"
#include "EscortWorkshopPanel.h"
#include "shader/FillShader.h"
#include "Firmware.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "GameData.h"
#include "Government.h"
#include "MissileGuidance.h"
#include "MouseButton.h"
#include "Outfit.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Screen.h"
#include "Ship.h"
#include "SimulatorPanel.h"
#include "UI.h"
#include "Weapon.h"
#include "text/WrappedText.h"

#include <algorithm>
#include <map>

using namespace std;

namespace {
	const double WIDTH = 760.;
	const double ROW_HEIGHT = 46.;
	const double OFFER_HEIGHT = 36.;
	const double CHOOSER_WIDTH = 250.;

	const Color BACKGROUND(.01f, .04f, .025f, .97f);
	const Color BORDER(.25f, .65f, .40f, 1.f);
	const Color TITLE(.75f, .95f, .80f, 1.f);
	const Color TEXT(.60f, .80f, .65f, 1.f);
	const Color DIM(.40f, .55f, .45f, 1.f);
	const Color PROGRAMMED(.55f, .80f, 1.f, 1.f);
	const Color BOUGHT(.95f, .80f, .45f, 1.f);
	const Color ROW(.03f, .08f, .05f, 1.f);
	const Color HOVER(.06f, .14f, .09f, 1.f);
	const Color TAB(.05f, .12f, .08f, 1.f);

	// The ship model the simulator fires at first.
	const string TARGET = "Argosy";
	// The maneuver the player's escorts fly, which their programs replace.
	const string KEEP_STATION = "keep station";

	// Shorten text to fit a width, with an ellipsis.
	string Fit(const Font &font, string text, double width)
	{
		if(font.Width(text) <= width)
			return text;
		while(!text.empty() && font.Width(text + "...") > width)
			text.pop_back();
		return text + "...";
	}

	// What hardware a guidance program needs, if any.
	string Needs(const MissileGuidance &guidance)
	{
		vector<string> needs;
		if(!guidance.MissingCapability(false, true, true).empty())
			needs.push_back("leading");
		if(!guidance.MissingCapability(true, false, true).empty())
			needs.push_back("throttle control");
		if(!guidance.MissingCapability(true, true, false).empty())
			needs.push_back("optical");
		if(needs.empty())
			return "runs on any seeker";
		string text = "needs";
		for(size_t i = 0; i < needs.size(); ++i)
			text += (i ? " and " : " ") + needs[i];
		return text;
	}
}



WorkshopPanel::WorkshopPanel(PlayerInfo &player)
	: player(player)
{
	SetInterruptible(false);
	escortRow.kind = "maneuver";
	daemonRow.kind = "daemon";
	FindWeapons();
}



void WorkshopPanel::Step()
{
	// Explain the workshop the first time it opens.
	DoHelp("workshop");
	if(statusTime > 0)
		--statusTime;
}



void WorkshopPanel::Draw()
{
	DrawBackdrop();
	// Weapons may have been programmed since this was last drawn.
	FindWeapons();
	FindEscorts();

	const Font &font = FontSet::Get(14);
	const Font &bigFont = FontSet::Get(18);
	const size_t lines = showExchange ? (GameData::Firmwares().size() + 1) / 2 + 3
		: max<size_t>(1, rows.size()) + 4 + roleRows.size();
	const double height = min(Screen::Height() - 20., 190. + lines * (showExchange ? OFFER_HEIGHT : ROW_HEIGHT)
		+ (showExchange ? 60. : 0.));
	const Rectangle box(Point(), Point(WIDTH, height));
	FillShader::Fill(box.Center(), box.Dimensions(), BACKGROUND);
	FillShader::Fill(Point(box.Center().X(), box.Top() + 1.), Point(WIDTH, 2.), BORDER);

	double y = box.Top() + 20.;
	bigFont.Draw("Missile Workshop", Point(box.Left() + 24., y), TITLE);
	const string credits = Format::CreditString(player.Accounts().Credits(), false);
	font.Draw(credits, Point(box.Right() - 60. - font.Width(credits), y + 3.), DIM);
	closeBox = Rectangle::WithCorners(Point(box.Right() - 40., box.Top() + 10.),
		Point(box.Right() - 10., box.Top() + 40.));
	font.Draw("x", closeBox.Center() - Point(font.Width("x") / 2., 8.), DIM);
	y += 34.;

	// Tabs: the fleet's programs, and the exchange.
	auto drawTab = [&](Rectangle &tab, const string &label, double left, bool active) {
		tab = Rectangle::WithCorners(Point(left, y), Point(left + font.Width(label) + 24., y + 24.));
		const bool hover = tab.Contains(UI::GetMouse());
		FillShader::Fill(tab.Center(), tab.Dimensions(), active ? HOVER : hover ? TAB : ROW);
		if(active)
			FillShader::Fill(Point(tab.Center().X(), tab.Bottom()), Point(tab.Width(), 2.), BORDER);
		font.Draw(label, tab.TopLeft() + Point(12., 5.), active ? TITLE : DIM);
	};
	drawTab(fleetTab, "Your fleet (f)", box.Left() + 16., !showExchange);
	drawTab(exchangeTab, "Program exchange (x)", fleetTab.Right() + 8., showExchange);
	y += 38.;

	if(showExchange)
		DrawExchange(box, y);
	else
		DrawFleet(box, y);

	if(statusTime > 0 && !status.empty())
		font.Draw(status, Point(box.Left() + 24., box.Bottom() - 26.), BOUGHT);
}



bool WorkshopPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(key == SDLK_ESCAPE || key == 'w' || command.Has(Command::MENU))
		GetUI().Pop(this);
	else if(key == 'x' || key == SDLK_TAB)
		showExchange = (key == 'x') ? true : !showExchange;
	else if(key == 'f')
		showExchange = false;
	else if(showExchange && key >= '1' && key <= '9')
	{
		// A number buys the program listed there.
		const size_t index = key - '1';
		auto it = GameData::Firmwares().begin();
		for(size_t i = 0; i < index && it != GameData::Firmwares().end(); ++i)
			++it;
		if(it != GameData::Firmwares().end())
			Buy(it->second);
	}
	else if(!showExchange && key >= '1' && key <= '9' && static_cast<size_t>(key - '1') < rows.size())
	{
		// Shift and a number chooses the weapon's next program; a number alone
		// opens it in the simulator.
		if(mod & KMOD_SHIFT)
			Cycle(rows[key - '1'], 1);
		else
			Open(*rows[key - '1'].outfit);
	}
	else if(!showExchange && key == 'e')
	{
		if(mod & KMOD_SHIFT)
			Cycle(escortRow, 1);
		else
			OpenEscorts();
	}
	else if(!showExchange && key == 'h')
		Cycle(daemonRow, 1);
	else if(!showExchange && key == 'r' && !roleRows.empty())
		Cycle(roleRows.front(), (mod & KMOD_SHIFT) ? -1 : 1);
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
	if(fleetTab.Contains(point))
	{
		showExchange = false;
		return true;
	}
	if(exchangeTab.Contains(point))
	{
		showExchange = true;
		return true;
	}
	if(showExchange)
	{
		for(const Offer &offer : offers)
			if(offer.box.Contains(point))
			{
				Buy(*offer.firmware);
				break;
			}
		return true;
	}
	vector<Row *> all;
	for(Row &row : rows)
		all.push_back(&row);
	all.push_back(&escortRow);
	all.push_back(&daemonRow);
	for(Row &row : roleRows)
		all.push_back(&row);
	for(Row *row : all)
	{
		if(row->previous.Contains(point))
			Cycle(*row, -1);
		else if(row->next.Contains(point))
			Cycle(*row, 1);
		else if(row->kind == "daemon" && row->box.Contains(point))
			Cycle(*row, 1);
		else if(row->kind == "role" && row->box.Contains(point))
			OpenEscorts(row->uuid);
		else if(row->box.Contains(point))
		{
			if(row->outfit)
				Open(*row->outfit);
			else
				OpenEscorts();
		}
		else
			continue;
		return true;
	}
	return true;
}



void WorkshopPanel::DrawFleet(const Rectangle &box, double y)
{
	const Font &font = FontSet::Get(14);
	font.Draw("Every guided missile runs a program that decides how it steers. Choose a weapon to",
		Point(box.Left() + 24., y), TEXT);
	y += 18.;
	font.Draw("write and test its program, or pick one you own with the arrows (shift+number).",
		Point(box.Left() + 24., y), TEXT);
	y += 30.;

	if(rows.empty())
	{
		font.Draw("None of your ships carries a guided weapon. Missile launchers are sold at outfitters.",
			Point(box.Left() + 24., y + 12.), DIM);
		y += ROW_HEIGHT;
	}
	int index = 0;
	for(Row &row : rows)
	{
		row.box = Rectangle::WithCorners(Point(box.Left() + 16., y), Point(box.Right() - 16., y + ROW_HEIGHT - 6.));
		const Weapon &weapon = *row.outfit->GetWeapon();
		string hardware = "hardware:";
		hardware += weapon.Leading() ? " leading" : "";
		hardware += weapon.ThrottleControl() ? " throttle" : "";
		hardware += weapon.OpticalTracking() ? " optical" : "";
		hardware += weapon.HasBlindspot() ? " blindspot" : "";
		if(hardware == "hardware:")
			hardware += " basic seeker";
		DrawRow(row, to_string(++index) + ".  " + row.outfit->DisplayName()
			+ (row.count > 1 ? "  x" + to_string(row.count) : ""), hardware);
		y += ROW_HEIGHT;
	}
	y += 20.;
	escortRow.box = Rectangle::WithCorners(Point(box.Left() + 16., y), Point(box.Right() - 16., y + ROW_HEIGHT - 6.));
	DrawRow(escortRow, "e.  Escort flight: keeping station", "how your escorts hold position near your ship");
	y += ROW_HEIGHT;
	daemonRow.box = Rectangle::WithCorners(Point(box.Left() + 16., y), Point(box.Right() - 16., y + ROW_HEIGHT - 6.));
	const Ship *flagship = player.Flagship();
	const bool hasDaemon = flagship && flagship->Attributes().Get("defense daemon") > 0.;
	DrawRow(daemonRow, "h.  Defense daemon", hasDaemon ? "which incoming missile your code injector hacks"
		: "needs a Defense Daemon outfit on your flagship");
	y += ROW_HEIGHT + 10.;
	// Roles: one for all escorts, and each escort's own, if it has one.
	font.Draw("Escort roles: what each escort does in a fight. Click one to try it against a raider; r changes"
		" all.",
		Point(box.Left() + 24., y), TEXT);
	y += 22.;
	for(Row &row : roleRows)
	{
		row.box = Rectangle::WithCorners(Point(box.Left() + 16., y), Point(box.Right() - 16., y + ROW_HEIGHT - 6.));
		const bool all = (row.uuid == "*");
		DrawRow(row, all ? "r.  All escorts" : "     " + row.name, all
			? "the role of every escort without one of its own" : "this escort's own role");
		y += ROW_HEIGHT;
	}
	y += 6.;
	const size_t library = player.ProgramLibrary().size();
	const size_t owned = player.OwnedFirmware().size();
	font.Draw("Your library: " + to_string(library) + (library == 1 ? " program" : " programs") + " you wrote, "
		+ to_string(owned) + " bought. More at the program exchange (x).", Point(box.Left() + 24., y), DIM);
}



void WorkshopPanel::DrawRow(Row &row, const string &title, const string &subtitle)
{
	const Font &font = FontSet::Get(14);
	const Point mouse = UI::GetMouse();
	const bool hover = row.box.Contains(mouse);
	FillShader::Fill(row.box.Center(), row.box.Dimensions(), hover ? HOVER : ROW);
	font.Draw(title, row.box.TopLeft() + Point(10., 4.), TITLE);
	font.Draw(subtitle, row.box.TopLeft() + Point(10., 21.), DIM);

	// The program chooser: < name >.
	const vector<Choice> choices = Choices(row);
	const int current = Current(row, choices);
	const double right = row.box.Right() - 8.;
	const double left = right - CHOOSER_WIDTH;
	const double top = row.box.Top() + 6.;
	row.previous = Rectangle::WithCorners(Point(left, top), Point(left + 24., top + 28.));
	row.next = Rectangle::WithCorners(Point(right - 24., top), Point(right, top + 28.));
	const bool more = choices.size() > 1;
	for(const Rectangle *arrow : {&row.previous, &row.next})
	{
		const bool over = more && arrow->Contains(mouse);
		FillShader::Fill(arrow->Center(), arrow->Dimensions(), over ? BORDER : TAB);
	}
	const Color &arrowColor = more ? TITLE : DIM;
	font.Draw("<", row.previous.Center() - Point(font.Width("<") / 2., 8.), arrowColor);
	font.Draw(">", row.next.Center() - Point(font.Width(">") / 2., 8.), arrowColor);

	string name;
	string kind;
	Color color = DIM;
	if(current < 0)
	{
		name = Firmware::NameOf(Installed(row));
		kind = "installed";
		color = PROGRAMMED;
	}
	else
	{
		const Choice &choice = choices[current];
		name = choice.name;
		if(choice.text.empty())
			kind = "factory";
		else if(player.ProgramLibrary().contains(choice.name))
		{
			kind = "your program";
			color = PROGRAMMED;
		}
		else
		{
			const Firmware *firmware = GameData::Firmwares().Find(choice.name);
			kind = firmware ? "by " + firmware->Author() : "bought";
			color = BOUGHT;
		}
	}
	const double width = CHOOSER_WIDTH - 56.;
	name = Fit(font, name, width);
	font.Draw(name, Point(left + CHOOSER_WIDTH / 2. - font.Width(name) / 2., top), color);
	const string sub = kind + (more ? "  (" + to_string(current + 1) + "/" + to_string(choices.size()) + ")" : "");
	font.Draw(sub, Point(left + CHOOSER_WIDTH / 2. - font.Width(sub) / 2., top + 15.), DIM);
}



void WorkshopPanel::DrawExchange(const Rectangle &box, double y)
{
	const Font &font = FontSet::Get(14);
	const Point mouse = UI::GetMouse();
	const Planet *planet = player.GetPlanet();
	const Government *government = planet ? planet->GetGovernment() : nullptr;
	font.Draw("Programs the galaxy's factions wrote, sold where their authors are. Click (or a number) to buy.",
		Point(box.Left() + 24., y), TEXT);
	y += 18.;
	font.Draw(player.Conditions().Get("start: missile lab")
		? "As a Missile Lab pilot, you can buy any of them here."
		: "Programs you buy can be chosen for any weapon whose hardware runs them.",
		Point(box.Left() + 24., y), DIM);
	y += 28.;

	offers.clear();
	const Firmware *hovered = nullptr;
	// A long list goes in two columns, read down each one.
	const int count = GameData::Firmwares().size();
	const int columns = count > 10 ? 2 : 1;
	const int perColumn = (count + columns - 1) / columns;
	const double columnWidth = (box.Width() - 32. - 8. * (columns - 1)) / columns;
	const double top = y;
	for(const auto &[name, firmware] : GameData::Firmwares())
	{
		Offer offer;
		offer.firmware = &firmware;
		const int index = offers.size();
		const double left = box.Left() + 16. + (index / perColumn) * (columnWidth + 8.);
		y = top + (index % perColumn) * OFFER_HEIGHT;
		offer.box = Rectangle::WithCorners(Point(left, y), Point(left + columnWidth, y + OFFER_HEIGHT - 2.));
		const bool hover = offer.box.Contains(mouse);
		if(hover)
			hovered = &firmware;
		FillShader::Fill(offer.box.Center(), offer.box.Dimensions(), hover ? HOVER : ROW);
		const bool owned = player.OwnsFirmware(name);
		const bool sold = IsSoldHere(firmware);
		const string price = owned ? "owned" : sold ? Format::CreditString(firmware.Price()) : "not sold here";
		const string label = (offers.size() < 9 ? to_string(offers.size() + 1) + ".  " : string()) + name;
		font.Draw(Fit(font, label, columnWidth - 30. - font.Width(price)), offer.box.TopLeft() + Point(10., 1.),
			owned ? BOUGHT : sold ? TITLE : DIM);
		string about = "by " + firmware.Author() + "  -  ";
		if(firmware.IsDaemon())
			about += "defense daemon";
		else if(firmware.Kind() == "role")
			about += "escort role";
		else if(firmware.IsGuidance())
		{
			const MissileGuidance *guidance = GameData::Guidance().Find(name);
			about += "missile guidance, " + (guidance ? Needs(*guidance) : string("missing its script"));
		}
		else
			about += "escort flight (replaces " + firmware.Replaces() + ")";
		font.Draw(Fit(font, about, columnWidth - 20.), offer.box.TopLeft() + Point(10., 17.), DIM);
		const bool affordable = player.Accounts().Credits() >= firmware.Price();
		font.Draw(price, Point(offer.box.Right() - 10. - font.Width(price), offer.box.Top() + 1.),
			owned ? BOUGHT : sold && affordable ? TEXT : DIM);
		offers.push_back(offer);
	}

	// What the program under the mouse does.
	y = top + perColumn * OFFER_HEIGHT + 8.;
	if(hovered)
	{
		WrappedText wrap(font);
		wrap.SetWrapWidth(WIDTH - 48.);
		wrap.Wrap(hovered->Description());
		wrap.Draw(Point(box.Left() + 24., y), TEXT);
	}
	else if(government && !player.Conditions().Get("start: missile lab"))
		font.Draw("This is " + government->DisplayName() + " space.", Point(box.Left() + 24., y), DIM);
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
			{
				Row row;
				row.outfit = outfit;
				row.count = count;
				found.push_back(row);
			}
			else
				it->count += count;
		}
	}
	// Keep where the rows were drawn, for clicks.
	for(Row &row : found)
		for(const Row &old : rows)
			if(old.outfit == row.outfit)
			{
				row.box = old.box;
				row.previous = old.previous;
				row.next = old.next;
			}
	rows = std::move(found);
}



void WorkshopPanel::FindEscorts()
{
	// A row for all escorts, then one for each escort here (as many as fit).
	vector<Row> found;
	Row all;
	all.kind = "role";
	all.uuid = "*";
	found.push_back(all);
	const Ship *flagship = player.Flagship();
	for(const shared_ptr<Ship> &ship : player.Ships())
	{
		if(found.size() > 6)
			break;
		if(ship.get() == flagship || ship->IsDestroyed() || ship->IsParked() || ship->CanBeCarried())
			continue;
		Row row;
		row.kind = "role";
		row.uuid = ship->UUID().ToString();
		row.name = ship->GivenName().empty() ? ship->DisplayModelName() : ship->GivenName();
		row.name += " (" + ship->DisplayModelName() + ")";
		found.push_back(row);
	}
	// Keep where the rows were drawn, for clicks.
	for(Row &row : found)
		for(const Row &old : roleRows)
			if(old.uuid == row.uuid)
			{
				row.box = old.box;
				row.previous = old.previous;
				row.next = old.next;
			}
	roleRows = std::move(found);
}



vector<WorkshopPanel::Choice> WorkshopPanel::Choices(const Row &row) const
{
	// The factory's program first, then the ones the player bought, then the
	// ones they wrote: those that can run on this weapon.
	vector<Choice> choices;
	const Outfit *outfit = row.outfit;
	if(outfit)
	{
		const Weapon &weapon = *outfit->GetWeapon();
		const MissileGuidance *factory = weapon.Guidance() ? weapon.Guidance() : MissileGuidance::Default(weapon);
		choices.push_back({factory ? factory->Name() : string("factory"), ""});
	}
	else if(row.kind == "maneuver")
		choices.push_back({KEEP_STATION, ""});
	else if(row.kind == "role")
	{
		// No role, then the roles that come with the game.
		choices.push_back({row.uuid == "*" ? "no role" : "as all escorts", ""});
		for(const auto &[name, role] : GameData::Roles())
			if(role.IsValid() && !GameData::Firmwares().Has(name))
				choices.push_back({name, GameData::ScriptText("role", name)});
	}
	else
	{
		const DefenseDaemon *factory = DefenseDaemon::Factory();
		choices.push_back({factory ? factory->Name() : string("factory"), ""});
	}
	auto fits = [&](const string &text) {
		if(Firmware::KindOf(text) != row.kind)
			return false;
		return !outfit || MissileGuidance::CanRun(text, *outfit->GetWeapon());
	};
	for(const string &name : player.OwnedFirmware())
	{
		const Firmware *firmware = GameData::Firmwares().Find(name);
		if(!firmware || firmware->Kind() != row.kind)
			continue;
		if(row.kind == "maneuver" && firmware->Replaces() != KEEP_STATION)
			continue;
		const string text = GameData::ScriptText(row.kind, name);
		if(fits(text))
			choices.push_back({name, text});
	}
	for(const auto &[name, text] : player.ProgramLibrary())
		if(fits(text))
			choices.push_back({name, text});
	return choices;
}



string WorkshopPanel::Installed(const Row &row) const
{
	if(row.outfit)
	{
		auto it = player.GuidancePrograms().find(row.outfit);
		return it == player.GuidancePrograms().end() ? string() : it->second;
	}
	if(row.kind == "maneuver")
	{
		auto it = player.ManeuverPrograms().find(KEEP_STATION);
		return it == player.ManeuverPrograms().end() ? string() : it->second;
	}
	if(row.kind == "role")
	{
		auto it = player.EscortRoles().find(row.uuid);
		return it == player.EscortRoles().end() ? string() : it->second;
	}
	return player.DaemonProgram();
}



bool WorkshopPanel::Install(const Row &row, const string &text)
{
	if(row.outfit)
		return player.SetGuidanceProgram(row.outfit, text);
	if(row.kind == "maneuver")
		return player.SetManeuverProgram(KEEP_STATION, text);
	if(row.kind == "role")
		return player.SetEscortRole(row.uuid, text);
	return player.SetDaemonProgram(text);
}



int WorkshopPanel::Current(const Row &row, const vector<Choice> &choices) const
{
	const string installed = Installed(row);
	for(size_t i = 0; i < choices.size(); ++i)
		if(choices[i].text == installed)
			return i;
	return -1;
}



void WorkshopPanel::Cycle(const Row &row, int direction)
{
	const vector<Choice> choices = Choices(row);
	if(choices.size() < 2)
	{
		status = "You have no other programs for this. Buy some at the program exchange (x), or write one.";
		statusTime = 300;
		return;
	}
	int current = Current(row, choices);
	// A program that isn't among the choices (edited, not yet in the library)
	// is kept there before it is replaced, so it can't be lost.
	if(current < 0)
	{
		const string text = Installed(row);
		string name = Firmware::NameOf(text);
		if(name.empty())
			name = "Unnamed program";
		player.SetLibraryProgram(name, text);
		current = 0;
	}
	const int count = choices.size();
	const Choice &choice = choices[((current + direction) % count + count) % count];
	if(!Install(row, choice.text))
		status = "That program doesn't run on this.";
	else if(row.outfit)
		status = "Your " + row.outfit->DisplayName() + "s now fly " + choice.name + ".";
	else if(row.kind == "maneuver")
		status = "Your escorts now keep station with " + choice.name + ".";
	else if(row.kind == "role" && row.uuid == "*")
		status = choice.text.empty() ? "Your escorts have no role: they fight as escorts usually do."
			: "Your escorts (those without a role of their own) now play " + choice.name + ".";
	else if(row.kind == "role")
		status = row.name + (choice.text.empty() ? " now plays the role all your escorts do."
			: " now plays " + choice.name + ".");
	else
		status = "Your defense daemon now runs " + choice.name + ".";
	statusTime = 300;
}



bool WorkshopPanel::IsSoldHere(const Firmware &firmware) const
{
	if(player.Conditions().Get("start: missile lab"))
		return true;
	const Planet *planet = player.GetPlanet();
	return planet && firmware.IsSoldBy(planet->GetGovernment());
}



void WorkshopPanel::Buy(const Firmware &firmware)
{
	if(player.OwnsFirmware(firmware.Name()))
	{
		status = "You already own " + firmware.Name() + ". Choose it for a weapon in your fleet (f).";
		statusTime = 300;
		return;
	}
	if(!IsSoldHere(firmware))
	{
		status = firmware.Name() + " is sold in " + firmware.Author() + " space.";
		statusTime = 300;
		return;
	}
	if(!player.BuyFirmware(firmware))
	{
		status = "You can't afford " + firmware.Name() + ".";
		statusTime = 300;
		return;
	}
	status = "Bought " + firmware.Name() + ". Choose it for " + (firmware.IsGuidance() ? string("a weapon")
		: firmware.IsDaemon() ? string("your defense daemon") : string("your escorts")) + " in your fleet (f).";
	statusTime = 360;
}



void WorkshopPanel::OpenEscorts(const string &roleUuid)
{
	// The player's flagship leads; the escort is the one whose role this is,
	// or one of the player's own, if they have any, or else a light warship.
	const Ship *leader = player.Flagship();
	const Ship *escort = nullptr;
	for(const shared_ptr<Ship> &ship : player.Ships())
		if(ship.get() != leader && !ship->IsDestroyed() && !ship->IsParked() && !ship->CanBeCarried()
				&& (roleUuid.empty() || roleUuid == "*" || ship->UUID().ToString() == roleUuid))
		{
			escort = ship.get();
			break;
		}
	if(!escort)
		escort = GameData::Ships().Find("Sparrow");
	if(!leader)
		leader = GameData::Ships().Find("Argosy");
	if(leader && escort)
		GetUI().Push(new EscortWorkshopPanel(&player, *leader, *escort, roleUuid));
}



void WorkshopPanel::Open(const Outfit &outfit)
{
	const Ship *target = GameData::Ships().Find(TARGET);
	if(target)
		GetUI().Push(new SimulatorPanel(outfit, *target, &player));
}
