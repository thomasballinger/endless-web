/* Editor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef EDITOR_H_
#define EDITOR_H_

#include "PlanetEditor.h"
#include "ShipEditor.h"
#include "SystemEditor.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class Body;
class Engine;
class PlayerInfo;
class Sprite;
class StellarObject;
class UI;



// Class representing the editor UI.
class Editor {
public:
	Editor(PlayerInfo &player, UI &ui) noexcept;
	Editor(const Editor &) = delete;
	Editor& operator=(const Editor &) = delete;
	~Editor();

	void RenderMain();

	bool HasPlugin() const;
	const std::string &GetPluginPath() const;
	PlayerInfo &Player();
	UI &GetUI();


private:
	void NewPlugin(const std::string &plugin);
	void OpenPlugin(const std::string &plugin);


private:
	PlayerInfo &player;
	UI &ui;

	PlanetEditor planetEditor;
	ShipEditor shipEditor;
	SystemEditor systemEditor;

	std::string currentPlugin;
	std::string currentPluginName;

	bool showShipMenu = false;
	bool showSystemMenu = false;
	bool showPlanetMenu = false;

	std::unordered_map<std::string, std::vector<std::string>> pluginPaths;
};



#endif
