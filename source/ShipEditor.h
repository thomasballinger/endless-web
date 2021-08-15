/* ShipEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SHIP_EDITOR_H_
#define SHIP_EDITOR_H_

#include "Ship.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

class DataWriter;
class Editor;



// Class representing the ship editor window.
class ShipEditor {
public:
	ShipEditor(Editor &editor, bool &show) noexcept;
	ShipEditor(const ShipEditor &) = delete;
	ShipEditor& operator=(const ShipEditor &) = delete;

	const std::vector<Ship> &Ships() const;
	void Render();
	void WriteToFile(DataWriter &writer, const Ship *ship);
	void WriteToPlugin(const Ship *ship);


private:
	void RenderShip();
	void RenderElement(Body *sprite, const std::string &name = "sprite") const;


private:
	Editor &editor;
	bool &showShipMenu;

	std::string searchBox;
	// Poor man's std::optional.
	std::unique_ptr<Ship> ship;

	std::set<const Ship *> dirty;
	std::vector<Ship> ships;
	std::vector<std::string> originalShips;
};



#endif
