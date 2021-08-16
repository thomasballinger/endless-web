/* PlanetEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "PlanetEditor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Editor.h"
#include "Effect.h"
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



PlanetEditor::PlanetEditor(Editor &editor, bool &show) noexcept
	: editor(editor), showPlanetMenu(show)
{
}



const list<Planet> &PlanetEditor::Planets() const
{
	return planets;
}



const set<const Planet *> &PlanetEditor::Dirty() const
{
	return dirty;
}



void PlanetEditor::Render()
{
	if(dirty.count(planet))
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Planet Editor", &showPlanetMenu))
	{
		ImGui::End();
		return;
	}

	if(dirty.count(planet))
		ImGui::PopStyleColor(3);

	if(ImGui::InputText("planet", &searchBox))
	{
		if(auto *ptr = GameData::Planets().Find(searchBox))
		{
			planet = const_cast<Planet *>(ptr);
			searchBox.clear();
		}
	}
	if(!planet || !dirty.count(planet))
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!planet || !dirty.count(planet))
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!planet)
				ImGui::SetTooltip("Select a planet first.");
			else if(!dirty.count(planet))
				ImGui::SetTooltip("No changes to reset.");
		}
	}
	ImGui::SameLine();
	if(!planet || searchBox.empty())
		ImGui::PushDisabled();
	bool clone = ImGui::Button("Clone");
	if(!planet || searchBox.empty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(searchBox.empty())
				ImGui::SetTooltip("Input the new name for the planet above.");
			else if(!planet)
				ImGui::SetTooltip("Select a planet first.");
		}
	}
	ImGui::SameLine();
	if(!planet || !editor.HasPlugin() || !dirty.count(planet))
		ImGui::PushDisabled();
	bool save = ImGui::Button("Save");
	if(!planet || !editor.HasPlugin() || !dirty.count(planet))
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!planet)
				ImGui::SetTooltip("Select a planet first.");
			else if(!editor.HasPlugin())
				ImGui::SetTooltip("Load a plugin to save to a file.");
			else if(!dirty.count(planet))
				ImGui::SetTooltip("No changes to save.");
		}
	}

	if(!planet)
	{
		ImGui::End();
		return;
	}

	if(reset)
	{
		*planet = *GameData::defaultPlanets.Get(planet->name);
		dirty.erase(planet);
	} 
	if(clone)
	{
		auto *clone = const_cast<Planet *>(GameData::Planets().Get(searchBox));
		*clone = *planet;
		planet = clone;

		planet->name = searchBox;
		searchBox.clear();
		dirty.insert(planet);
	}
	if(save)
		WriteToPlugin(planet);

	ImGui::Separator();
	ImGui::Spacing();
	RenderPlanet();
	ImGui::End();
}



void PlanetEditor::RenderPlanet()
{
	int index = 0;

	ImGui::Text("name: %s", planet->name.c_str());

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : planet->attributes)
		{
			if(attribute == "spaceport" || attribute == "shipyard" || attribute == "outfitter")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
		{
			planet->attributes.insert(attribute);
			if(attribute.compare(0, 10, "requires: "))
				planet->requiredAttributes.insert(attribute.substr(10));
		}
		for(auto &&attribute : toRemove)
		{
			planet->attributes.erase(attribute);
			if(attribute.compare(0, 10, "requires: "))
				planet->requiredAttributes.erase(attribute.substr(10));
		}
		if(!toAdd.empty() || !toRemove.empty())
			dirty.insert(planet);

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##planet", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			planet->attributes.insert(addAttribute);
			dirty.insert(planet);
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("shipyards"))
	{
		index = 0;
		const Sale<Ship> *toAdd = nullptr;
		const Sale<Ship> *toRemove = nullptr;
		for(auto it = planet->shipSales.begin(); it != planet->shipSales.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::BeginCombo("shipyard", (*it)->name.c_str()))
			{
				for(const auto &item : GameData::Shipyards())
				{
					const bool selected = &item.second == *it;
					if(ImGui::Selectable(item.first.c_str(), selected))
					{
						toAdd = &item.second;
						toRemove = *it;
						dirty.insert(planet);
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}

				if(ImGui::Selectable("[remove]"))
				{
					toRemove = *it;
					dirty.insert(planet);
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		if(toAdd)
			planet->shipSales.insert(toAdd);
		if(toRemove)
			planet->shipSales.erase(toRemove);
		if(ImGui::BeginCombo("add shipyard", ""))
		{
			for(const auto &item : GameData::Shipyards())
				if(ImGui::Selectable(item.first.c_str()))
				{
					planet->shipSales.insert(&item.second);
					dirty.insert(planet);
				}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("outfitters"))
	{
		index = 0;
		const Sale<Outfit> *toAdd = nullptr;
		const Sale<Outfit> *toRemove = nullptr;
		for(auto it = planet->outfitSales.begin(); it != planet->outfitSales.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::BeginCombo("outfitter", (*it)->name.c_str()))
			{
				for(const auto &item : GameData::Outfitters())
				{
					const bool selected = &item.second == *it;
					if(ImGui::Selectable(item.first.c_str(), selected))
					{
						toAdd = &item.second;
						toRemove = *it;
						dirty.insert(planet);
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}

				if(ImGui::Selectable("[remove]"))
				{
					toRemove = *it;
					dirty.insert(planet);
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		if(toAdd)
			planet->outfitSales.insert(toAdd);
		if(toRemove)
			planet->outfitSales.erase(toRemove);
		if(ImGui::BeginCombo("add outfitter", ""))
		{
			for(const auto &item : GameData::Outfitters())
				if(ImGui::Selectable(item.first.c_str()))
				{
					planet->outfitSales.insert(&item.second);
					dirty.insert(planet);
				}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("tribute"))
	{
		if(ImGui::InputInt("tribute", &planet->tribute))
			dirty.insert(planet);
		if(ImGui::InputInt("threshold", &planet->defenseThreshold))
			dirty.insert(planet);

		if(ImGui::TreeNode("fleets"))
		{
			index = 0;
			map<const Fleet *, int> modify;
			for(auto it = planet->defenseFleets.begin(); it != planet->defenseFleets.end();)
			{
				int count = 1;
				auto counter = it;
				while(next(counter) != planet->defenseFleets.end()
						&& (*next(counter))->Name() == (*counter)->Name())
				{
					++counter;
					++count;
				}

				ImGui::PushID(index);
				if(ImGui::BeginCombo("##fleets", (*it)->Name().c_str()))
				{
					for(const auto &item : GameData::Fleets())
					{
						const bool selected = &item.second == *it;
						if(ImGui::Selectable(item.first.c_str(), selected))
						{
							// We need to update every entry.
							auto begin = it;
							auto end = it + count;
							while(begin != end)
								*begin++ = &item.second;
							dirty.insert(planet);
						}

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				int oldCount = count;
				if(ImGui::InputInt("fleet", &count))
				{
					modify[*it] = count - oldCount;
					dirty.insert(planet);
				}

				++index;
				it += oldCount;
				ImGui::PopID();
			}

			// Now update any fleets entries.
			for(auto &&pair : modify)
			{
				auto first = find(planet->defenseFleets.begin(), planet->defenseFleets.end(), pair.first);
				if(pair.second <= 0)
					planet->defenseFleets.erase(first, first + (-pair.second));
				else
					planet->defenseFleets.insert(first, pair.second, pair.first);
			}

			// Now add the ability to add fleets.
			if(ImGui::BeginCombo("##fleets", ""))
			{
				for(const auto &item : GameData::Fleets())
					if(ImGui::Selectable(item.first.c_str()))
					{
						planet->defenseFleets.emplace_back(&item.second);
						dirty.insert(planet);
					}
				ImGui::EndCombo();
			}

			ImGui::TreePop();
		}
		ImGui::TreePop();
	}


	static string landscape;
	if(planet->landscape)
		landscape = planet->landscape->Name();
	if(ImGui::InputText("landscape", &landscape, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		planet->landscape = SpriteSet::Get(landscape);
		GameData::Preload(planet->landscape);
		dirty.insert(planet);
	}
	if(ImGui::InputText("music", &planet->music, ImGuiInputTextFlags_EnterReturnsTrue))
		dirty.insert(planet);

	if(ImGui::InputTextMultiline("description", &planet->description, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
		dirty.insert(planet);
	if(ImGui::InputTextMultiline("spaceport", &planet->spaceport, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		dirty.insert(planet);

		if(planet->spaceport.empty())
			planet->attributes.erase("spaceport");
		else
			planet->attributes.insert("spaceport");
	}

	{
		if(ImGui::BeginCombo("governments", planet->government ? planet->government->GetName().c_str() : ""))
		{
			int index = 0;
			for(const auto &government : GameData::Governments())
			{
				const bool selected = &government.second == planet->government;
				if(ImGui::Selectable(government.first.c_str(), selected))
				{
					planet->government = &government.second;
					dirty.insert(planet);
				}
				++index;

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if(ImGui::InputDoubleEx("required reputation", &planet->requiredReputation))
		dirty.insert(planet);
	if(ImGui::InputDoubleEx("bribe", &planet->bribe))
		dirty.insert(planet);
	if(ImGui::InputDoubleEx("security", &planet->security))
		dirty.insert(planet);
	planet->customSecurity = planet->security != .25;

	planet->inhabited = (!planet->spaceport.empty() || planet->requiredReputation || !planet->defenseFleets.empty()) && !planet->attributes.count("uninhabited");
}



void PlanetEditor::WriteToPlugin(const Planet *planet)
{
	if(!editor.HasPlugin())
		return;

	dirty.erase(planet);
	for(auto &&p : planets)
		if(p.name == planet->name)
		{
			p = *planet;
			return;
		}

	planets.push_back(*planet);
}



void PlanetEditor::WriteToFile(DataWriter &writer, const Planet *planet)
{
	writer.Write("planet", planet->name);
	writer.BeginChild();

	if(!planet->attributes.empty())
	{
		writer.WriteToken("attributes");
		for(auto &&attribute : planet->attributes)
			if(attribute != "spaceport" && attribute != "shipyard" && attribute != "outfitter")
				writer.WriteToken(attribute);
		writer.Write();
	}

	writer.Write("remove", "shipyard");
	for(auto &&shipyard : planet->shipSales)
		writer.Write("shipyard", shipyard->name);
	writer.Write("remove", "outfitter");
	for(auto &&outfitter : planet->outfitSales)
		writer.Write("outfitter", outfitter->name);
	if(planet->landscape)
		writer.Write("landscape", planet->landscape->Name());
	if(!planet->music.empty())
		writer.Write("music", planet->music);
	if(!planet->description.empty())
	{
		auto marker = planet->description.find('\n');
		size_t start = 0;
		do
		{
			writer.Write("description", planet->description.substr(start, marker - start));

			start = marker + 1;
			if(planet->description[start] == '\t')
				++start;
			marker = planet->description.find('\n', start);
		} while(marker != string::npos);
	}
	if(!planet->spaceport.empty())
	{
		auto marker = planet->spaceport.find('\n');
		size_t start = 0;
		do
		{
			writer.Write("spaceport", planet->spaceport.substr(start, marker - start));

			start = marker + 1;
			if(planet->spaceport[start] == '\t')
				++start;
			marker = planet->spaceport.find('\n', start);
		} while(marker != string::npos);
	}
	if(planet->government)
		writer.Write("government", planet->government->GetName());
	if(planet->requiredReputation)
		writer.Write("required reputation", planet->requiredReputation);
	if(planet->bribe)
		writer.Write("bribe", planet->bribe);
	if(planet->customSecurity)
		writer.Write("security", planet->security);
	if(planet->tribute)
	{
		writer.Write("tribute", planet->tribute);
		writer.BeginChild();
		if(planet->defenseThreshold != 4000.)
			writer.Write("threshold", planet->defenseThreshold);
		for(size_t i = 0; i < planet->defenseFleets.size(); ++i)
		{
			size_t count = 1;
			while(i + 1 < planet->defenseFleets.size()
					&& planet->defenseFleets[i + 1]->Name() == planet->defenseFleets[i]->Name())
			{
				++i;
				++count;
			}
			writer.Write("fleet", planet->defenseFleets[i]->Name(), count);
		}
		writer.EndChild();
	}

	writer.EndChild();
}



void PlanetEditor::WriteAll()
{
	auto copy = dirty;
	for(auto &&p : copy)
		WriteToPlugin(p);
}
