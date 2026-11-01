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
	// One kind of homing weapon in the fleet (or, with no outfit, how the
	// player's escorts keep station).
	class Row {
	public:
		const Outfit *outfit = nullptr;
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
	std::vector<Choice> Choices(const Outfit *outfit) const;
	// The index of the row's program among its choices, or -1 if it is one
	// that is not among them.
	int Current(const Outfit *outfit, const std::vector<Choice> &choices) const;
	void Cycle(const Outfit *outfit, int direction);
	void Open(const Outfit &outfit);
	// Open the escort workshop, for how the player's escorts keep station.
	void OpenEscorts();
	bool IsSoldHere(const Firmware &firmware) const;
	void Buy(const Firmware &firmware);

	void DrawFleet(const Rectangle &box, double y);
	void DrawExchange(const Rectangle &box, double y);
	void DrawRow(Row &row, const std::string &title, const std::string &subtitle);


private:
	PlayerInfo &player;
	std::vector<Row> rows;
	Row escortRow;
	std::vector<Offer> offers;
	bool showExchange = false;
	Rectangle closeBox;
	Rectangle fleetTab;
	Rectangle exchangeTab;
	std::string status;
	int statusTime = 0;
};
