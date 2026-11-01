/* WorkshopPanel.h
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

#pragma once

#include "Panel.h"

#include "Rectangle.h"

#include <string>
#include <vector>

class Firmware;
class Outfit;
class PlayerInfo;



// A missile workshop, open while landed somewhere with an outfitter. It lists
// the homing weapons in the player's fleet, and the program each flies: the
// factory's, one the player bought, or one from the player's library, chosen
// with the arrows beside it. Choosing a weapon opens it in the simulator,
// where the player can write and test a program and install it on all their
// ships' weapons of that kind. Escorts' flight programs are chosen the same
// way. The program exchange sells the programs the galaxy's factions wrote.
class WorkshopPanel : public Panel {
public:
	explicit WorkshopPanel(PlayerInfo &player);

	virtual void Step() override;
	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;


private:
	// One kind of homing weapon in the fleet, or (with no outfit) how the
	// player's escorts keep station, or the flagship's defense daemon.
	class Row {
	public:
		// "guidance", "maneuver", "daemon" or "role".
		std::string kind = "guidance";
		const Outfit *outfit = nullptr;
		// For a role: the escort's UUID ("*" for all escorts), and its name.
		std::string uuid;
		std::string name;
		int count = 0;
		Rectangle box;
		Rectangle previous;
		Rectangle next;
	};
	// A program the player can choose for a row: its name, and its text
	// (empty for the factory's).
	class Choice {
	public:
		std::string name;
		std::string text;
	};
	// A program for sale, as drawn.
	class Offer {
	public:
		const Firmware *firmware = nullptr;
		Rectangle box;
	};


private:
	void FindWeapons();
	void FindEscorts();
	std::vector<Choice> Choices(const Row &row) const;
	// The program the row's weapon (or escorts, or daemon) runs, if it isn't
	// the factory's.
	std::string Installed(const Row &row) const;
	bool Install(const Row &row, const std::string &text);
	// The index of the row's program among its choices, or -1 if it is one
	// that is not among them.
	int Current(const Row &row, const std::vector<Choice> &choices) const;
	void Cycle(const Row &row, int direction);
	void Open(const Outfit &outfit);
	// Open the escort workshop, for how the player's escorts keep station, or
	// for the role of an escort (or of all of them).
	void OpenEscorts(const std::string &roleUuid = "");
	bool IsSoldHere(const Firmware &firmware) const;
	void Buy(const Firmware &firmware);

	void DrawFleet(const Rectangle &box, double y);
	void DrawExchange(const Rectangle &box, double y);
	void DrawRow(Row &row, const std::string &title, const std::string &subtitle);


private:
	PlayerInfo &player;
	std::vector<Row> rows;
	Row escortRow;
	Row daemonRow;
	// A role for all escorts, then each escort's own.
	std::vector<Row> roleRows;
	std::vector<Offer> offers;
	bool showExchange = false;
	Rectangle closeBox;
	Rectangle fleetTab;
	Rectangle exchangeTab;
	std::string status;
	int statusTime = 0;
};
