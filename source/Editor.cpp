/* Editor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Editor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Effect.h"
#include "EsUuid.h"
#include "Engine.h"
#include "Files.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MainPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



Editor::Editor(PlayerInfo &player, UI &menu, UI &ui) noexcept
	: player(player), menu(menu), ui(ui),
	outfitEditor(*this, showOutfitMenu), planetEditor(*this, showPlanetMenu),
	shipEditor(*this, showShipMenu), systemEditor(*this, showSystemMenu)
{
}



Editor::~Editor()
{
	WriteAll();
}



// Saves every unsaved changes to the current plugin if any.
void Editor::SaveAll()
{
	if(!HasPlugin())
		return;

	// Commit to any unsaved changes.
	outfitEditor.WriteAll();
	shipEditor.WriteAll();
	planetEditor.WriteAll();
	systemEditor.WriteAll();
}



// Writes the plugin to a file.
void Editor::WriteAll()
{
	if(!HasPlugin())
		return;

	const auto &outfits = outfitEditor.Changes();
	const auto &planets = planetEditor.Changes();
	const auto &ships = shipEditor.Ships();
	const auto &systems = systemEditor.Changes();

	// Which object we have saved to file.
	set<string> outfitsSaved;
	set<string> planetsSaved;
	set<string> shipsSaved;
	set<string> systemsSaved;

	// Save every change made to this plugin.
	for(auto &&file : pluginPaths)
	{
		// Special case: default paths are saved later.
		const auto &fileName = Files::Name(file.first);
		if(fileName == "map.txt" || fileName == "ships.txt" || fileName == "outfits.txt")
			continue;

		DataWriter writer(file.first);

		for(auto &&objects : file.second)
		{
			string toSearch = objects.substr(1);
			if(objects[0] == '0')
			{
				auto it = find_if(planets.begin(), planets.end(),
						[&toSearch](const Planet &p) { return p.Name() == toSearch; });
				if(it != planets.end())
				{
					planetEditor.WriteToFile(writer, &*it);
					writer.Write();
					planetsSaved.insert(it->Name());
					continue;
				}
			}
			else if(objects[0] == '1')
			{
				auto it = find_if(ships.begin(), ships.end(),
						[&toSearch](const Ship &s) { return s.Name() == toSearch; });
				if(it != ships.end())
				{
					shipEditor.WriteToFile(writer, &*it);
					writer.Write();
					shipsSaved.insert(it->Name());
					continue;
				}
			}
			else if(objects[0] == '2')
			{
				auto it = find_if(systems.begin(), systems.end(),
						[&toSearch](const System &sys) { return sys.Name() == toSearch; });
				if(it != systems.end())
				{
					systemEditor.WriteToFile(writer, &*it);
					writer.Write();
					systemsSaved.insert(it->Name());
					continue;
				}
			}
			else if(objects[0] == '3')
			{
				auto it = find_if(outfits.begin(), outfits.end(),
						[&toSearch](const Outfit &o) { return o.Name() == toSearch; });
				if(it != outfits.end())
				{
					outfitEditor.WriteToFile(writer, &*it);
					writer.Write();
					outfitsSaved.insert(it->Name());
					continue;
				}
			}
			else
				assert(!"Invalid object type to write to file! Please report this.");
		}
	}

	// Save any new object to a default path.
	if(!planets.empty() || !systems.empty())
	{
		DataWriter mapTxt(currentPlugin + "data/map.txt");
		for(auto &&planet : planets)
			if(!planetsSaved.count(planet.Name()))
			{
				planetEditor.WriteToFile(mapTxt, &planet);
				mapTxt.Write();
			}
		for(auto &&system : systems)
			if(!systemsSaved.count(system.Name()))
			{
				systemEditor.WriteToFile(mapTxt, &system);
				mapTxt.Write();
			}
	}
	if(!ships.empty())
	{
		DataWriter shipsTxt(currentPlugin + "data/ships.txt");
		for(auto &&ship : ships)
			if(!shipsSaved.count(ship.Name()))
			{
				shipEditor.WriteToFile(shipsTxt, &ship);
				shipsTxt.Write();
			}
	}
	if(!outfits.empty())
	{
		DataWriter outfitsTxt(currentPlugin + "data/outfits.txt");
		for(auto &&outfit : outfits)
			if(!shipsSaved.count(outfit.Name()))
			{
				outfitEditor.WriteToFile(outfitsTxt, &outfit);
				outfitsTxt.Write();
			}
	}
}



bool Editor::HasPlugin() const
{
	return !currentPlugin.empty();
}



bool Editor::HasUnsavedChanges() const
{
	return
		!outfitEditor.Dirty().empty()
		|| !planetEditor.Dirty().empty()
		|| !shipEditor.Dirty().empty()
		|| !systemEditor.Dirty().empty();
}



const std::string &Editor::GetPluginPath() const
{
	return currentPlugin;
}



PlayerInfo &Editor::Player()
{
	return player;
}



UI &Editor::GetUI()
{
	return ui;
}



void Editor::RenderMain()
{
	if(showOutfitMenu)
		outfitEditor.Render();
	if(showShipMenu)
		shipEditor.Render();
	if(showSystemMenu)
		systemEditor.Render();
	if(showPlanetMenu)
		planetEditor.Render();

	bool newPluginDialog = false;
	bool openPluginDialog = false;
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New Plugin", nullptr, &newPluginDialog);
			ImGui::MenuItem("Open Plugin", nullptr, &openPluginDialog);
			if(!currentPlugin.empty())
				ImGui::MenuItem(("\"" + currentPluginName + "\" loaded").c_str(), nullptr, false, false);
			if(ImGui::MenuItem("Save All", nullptr, false, HasPlugin()))
			{
				SaveAll();
				WriteAll();
			}
			if(ImGui::MenuItem("Quit"))
				menu.Quit();
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Editors"))
		{
			ImGui::MenuItem("Outfit Editor", nullptr, &showOutfitMenu);
			ImGui::MenuItem("Ship Editor", nullptr, &showShipMenu);
			ImGui::MenuItem("System Editor", nullptr, &showSystemMenu);
			ImGui::MenuItem("Planet Editor", nullptr, &showPlanetMenu);
			ImGui::EndMenu();
		}

		const auto &dirtyOutfits = planetEditor.Dirty();
		const auto &dirtyPlanets = planetEditor.Dirty();
		const auto &dirtyShips = shipEditor.Dirty();
		const auto &dirtySystems = systemEditor.Dirty();
		const bool hasChanges = HasUnsavedChanges();

		if(hasChanges)
			ImGui::PushStyleColor(ImGuiCol_PopupBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		if(ImGui::BeginMenu("Unsaved Changes", hasChanges))
		{
			if(!dirtyOutfits.empty() && ImGui::BeginMenu("Outfits"))
			{
				for(auto &&o : dirtyOutfits)
					ImGui::MenuItem(o->Name().c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyPlanets.empty() && ImGui::BeginMenu("Planets"))
			{
				for(auto &&p : dirtyPlanets)
					ImGui::MenuItem(p->TrueName().c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyShips.empty() && ImGui::BeginMenu("Ships"))
			{
				for(auto &&s : dirtyShips)
					ImGui::MenuItem(s->Name().c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtySystems.empty() && ImGui::BeginMenu("Systems"))
			{
				for(auto &&sys : dirtySystems)
					ImGui::MenuItem(sys->Name().c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}
		if(hasChanges)
			ImGui::PopStyleColor();

		ImGui::EndMainMenuBar();
	}

	if(newPluginDialog)
		ImGui::OpenPopup("New Plugin");
	if(openPluginDialog)
		ImGui::OpenPopup("Open Plugin");
	if(showConfirmationDialog)
		ImGui::OpenPopup("Confirmation Dialog");

	if(ImGui::BeginPopupModal("New Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string newPlugin;
		ImGui::Text("Create new plugin:");
		bool create = ImGui::InputText("", &newPlugin, ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
		}
		ImGui::SameLine();
		if(newPlugin.empty())
			ImGui::PushDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || create)
		{
			NewPlugin(newPlugin);
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
			dontDisable = true;
		}
		if(newPlugin.empty() && !dontDisable)
			ImGui::PopDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Open Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string openPlugin;
		ImGui::Text("Open plugin:");
		bool create = ImGui::InputText("", &openPlugin, ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
		}
		ImGui::SameLine();
		if(openPlugin.empty())
			ImGui::PushDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || create)
		{
			OpenPlugin(openPlugin);
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
			dontDisable = true;
		}
		if(openPlugin.empty() && !dontDisable)
			ImGui::PopDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Confirmation Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes. Are you sure you want to quit?");
		if(ImGui::Button("No"))
		{
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Yes"))
		{
			menu.Quit();
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(!HasPlugin())
			ImGui::PushDisabled();
		if(ImGui::Button("Save All and Quit"))
		{
			menu.Quit();
			showConfirmationDialog = false;
			SaveAll();
			ImGui::CloseCurrentPopup();
		}
		if(!HasPlugin())
		{
			ImGui::PopDisabled();
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("You don't have a plugin loaded to save the changes to.");
		}
		ImGui::EndPopup();
	}
}



void Editor::ShowConfirmationDialog()
{
	if(HasUnsavedChanges())
		showConfirmationDialog = true;
	else
		// No unsaved changes, so just quit.
		menu.Quit();
}



void Editor::NewPlugin(const string &plugin)
{
	// Don't create a new plugin it if already exists.
	auto pluginsPath = Files::Config() + "plugins/";
	auto plugins = Files::ListDirectories(pluginsPath);
	for(const auto &existing : plugins)
		if(existing == plugin)
			return OpenPlugin(plugin);

	Files::CreateNewDirectory(pluginsPath + plugin);
	Files::CreateNewDirectory(pluginsPath + plugin + "/data");
	OpenPlugin(plugin);
}



void Editor::OpenPlugin(const string &plugin)
{
	const string path = Files::Config() + "plugins/" + plugin + "/";
	if(!Files::Exists(path))
		return;

	currentPlugin = path;
	currentPluginName = plugin;

	// We need to save everything the specified plugin loads.
	auto files = Files::RecursiveList(path + "data/");
	for(const auto &file : files)
	{
		DataFile data(file);
		for(const auto &node : data)
		{
			const string &key = node.Token(0);
			if(node.Size() < 2)
				continue;
			const string &value = node.Token(1);

			char num;
			if(key == "planet")
			{
				num = '0';
				planetEditor.WriteToPlugin(GameData::Planets().Get(value));
			}
			else if(key == "ship")
			{
				num = '1';
				shipEditor.WriteToPlugin(GameData::Ships().Get(value));
			}
			else if(key == "system")
			{
				num = '2';
				systemEditor.WriteToPlugin(GameData::Systems().Get(value));
			}
			else if(key == "outfit")
			{
				num = '3';
				outfitEditor.WriteToPlugin(GameData::Outfits().Get(value));
			}
			else
			{
				node.PrintTrace("node will get pruned when saving!");
				continue;
			}
			pluginPaths[file].emplace_back(num + value);
		}
	}
}
