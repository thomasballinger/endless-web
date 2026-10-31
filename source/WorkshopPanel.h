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

class Outfit;
class PlayerInfo;



// A missile workshop, open while landed somewhere with an outfitter. It lists
// the homing weapons in the player's fleet, and whether each flies the
// factory's guidance or a program the player wrote. Choosing one opens it in
// the simulator, where the player can write and test a program and install it
// on all their ships' weapons of that kind.
class WorkshopPanel : public Panel {
public:
	explicit WorkshopPanel(PlayerInfo &player);

	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;


private:
	// One kind of homing weapon in the fleet.
	class Row {
	public:
		const Outfit *outfit = nullptr;
		int count = 0;
		Rectangle box;
	};


private:
	void FindWeapons();
	void Open(const Outfit &outfit);


private:
	PlayerInfo &player;
	std::vector<Row> rows;
	Rectangle closeBox;
};
