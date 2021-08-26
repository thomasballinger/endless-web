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

#include "EffectEditor.h"
#include "FleetEditor.h"
#include "HazardEditor.h"
#include "GovernmentEditor.h"
#include "OutfitEditor.h"
#include "OutfitterEditor.h"
#include "PlanetEditor.h"
#include "ShipEditor.h"
#include "ShipyardEditor.h"
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
	Editor(PlayerInfo &player, UI &menu, UI &ui) noexcept;
	Editor(const Editor &) = delete;
	Editor& operator=(const Editor &) = delete;
	~Editor();

	// Saves every unsaved changes to the current plugin if any.
	void SaveAll();
	// Writes the plugin to a file.
	void WriteAll();

	void RenderMain();

	void ShowConfirmationDialog();
	void ReloadPluginResources();

	bool HasPlugin() const;
	bool HasUnsavedChanges() const;
	const std::string &GetPluginPath() const;
	PlayerInfo &Player();
	UI &GetUI();
	UI &GetMenu();


private:
	void NewPlugin(const std::string &plugin);
	void OpenPlugin(const std::string &plugin);

	void StyleColorsYellow();
	void StyleColorsDarkGray();


private:
	PlayerInfo &player;
	UI &menu;
	UI &ui;

	EffectEditor effectEditor;
	FleetEditor fleetEditor;
	HazardEditor hazardEditor;
	GovernmentEditor governmentEditor;
	OutfitEditor outfitEditor;
	OutfitterEditor outfitterEditor;
	PlanetEditor planetEditor;
	ShipEditor shipEditor;
	ShipyardEditor shipyardEditor;
	SystemEditor systemEditor;

	std::string currentPlugin;
	std::string currentPluginName;

	bool showConfirmationDialog = false;
	bool showEffectMenu = false;
	bool showFleetMenu = false;
	bool showHazardMenu = false;
	bool showGovernmentMenu = false;
	bool showOutfitMenu = false;
	bool showOutfitterMenu = false;
	bool showShipMenu = false;
	bool showShipyardMenu = false;
	bool showSystemMenu = false;
	bool showPlanetMenu = false;

	std::unordered_map<std::string, std::vector<std::string>> pluginPaths;
};



#endif
