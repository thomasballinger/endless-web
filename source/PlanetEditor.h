/* PlanetEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef PLANET_EDITOR_H_
#define PLANET_EDITOR_H_

#include "DataWriter.h"
#include "Ship.h"
#include "System.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

class Body;
class Engine;
class Editor;
class PlayerInfo;
class Sprite;
class StellarObject;
class UI;



// Class representing the planet editor window.
class PlanetEditor {
public:
	PlanetEditor(Editor &editor, bool &show) noexcept;
	PlanetEditor(const PlanetEditor &) = delete;
	PlanetEditor& operator=(const PlanetEditor &) = delete;

	const std::vector<Planet> &Planets() const;
	void Render();
	void WriteToFile(DataWriter &writer, const Planet *planet);
	void WriteToPlugin(const Planet *planet);

private:
	void RenderPlanetMenu();

	void RenderPlanet();



private:
	Editor &editor;
	bool &showPlanetMenu;

	std::string searchBox;
	Planet *planet = nullptr;

	std::set<const Planet *> dirty;
	std::vector<Planet> planets;
	std::vector<std::string> originalPlanets;
};



#endif
