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
	: TemplateEditor<Planet>(editor, show)
{
}



void PlanetEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Planet Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::InputCombo("planet", &searchBox, &object, GameData::Planets()))
		searchBox.clear();
	if(!object || !IsDirty())
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!object || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a planet first.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to reset.");
		}
	}
	ImGui::SameLine();
	if(!object || searchBox.empty())
		ImGui::PushDisabled();
	bool clone = ImGui::Button("Clone");
	if(!object || searchBox.empty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(searchBox.empty())
				ImGui::SetTooltip("Input the new name for the planet above.");
			else if(!object)
				ImGui::SetTooltip("Select a planet first.");
		}
	}
	ImGui::SameLine();
	if(!object || !editor.HasPlugin() || !IsDirty())
		ImGui::PushDisabled();
	bool save = ImGui::Button("Save");
	if(!object || !editor.HasPlugin() || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a planet first.");
			else if(!editor.HasPlugin())
				ImGui::SetTooltip("Load a plugin to save to a file.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to save.");
		}
	}

	if(!object)
	{
		ImGui::End();
		return;
	}

	if(reset)
	{
		*object = *GameData::defaultPlanets.Get(object->name);
		SetClean();
	} 
	if(clone)
	{
		auto *clone = const_cast<Planet *>(GameData::Planets().Get(searchBox));
		*clone = *object;
		object = clone;

		object->name = searchBox;
		object->systems.clear();
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderPlanet();
	ImGui::End();
}



void PlanetEditor::RenderPlanet()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "spaceport" || attribute == "shipyard" || attribute == "outfitter")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("##attribute", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
		{
			object->attributes.insert(attribute);
			if(attribute.size() > 10 && attribute.compare(0, 10, "requires: "))
				object->requiredAttributes.insert(attribute.substr(10));
		}
		for(auto &&attribute : toRemove)
		{
			object->attributes.erase(attribute);
			if(attribute.size() > 10 && attribute.compare(0, 10, "requires: "))
				object->requiredAttributes.erase(attribute.substr(10));
		}
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##planet", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->attributes.insert(addAttribute);
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("shipyards"))
	{
		index = 0;
		const Sale<Ship> *toAdd = nullptr;
		const Sale<Ship> *toRemove = nullptr;
		for(auto it = object->shipSales.begin(); it != object->shipSales.end(); ++it)
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
						SetDirty();
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}

				if(ImGui::Selectable("[remove]"))
				{
					toRemove = *it;
					SetDirty();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		if(toAdd)
			object->shipSales.insert(toAdd);
		if(toRemove)
			object->shipSales.erase(toRemove);
		if(ImGui::BeginCombo("add shipyard", ""))
		{
			for(const auto &item : GameData::Shipyards())
				if(ImGui::Selectable(item.first.c_str()))
				{
					object->shipSales.insert(&item.second);
					SetDirty();
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
		for(auto it = object->outfitSales.begin(); it != object->outfitSales.end(); ++it)
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
						SetDirty();
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}

				if(ImGui::Selectable("[remove]"))
				{
					toRemove = *it;
					SetDirty();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		if(toAdd)
			object->outfitSales.insert(toAdd);
		if(toRemove)
			object->outfitSales.erase(toRemove);
		if(ImGui::BeginCombo("add outfitter", ""))
		{
			for(const auto &item : GameData::Outfitters())
				if(ImGui::Selectable(item.first.c_str()))
				{
					object->outfitSales.insert(&item.second);
					SetDirty();
				}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("tribute"))
	{
		if(ImGui::InputInt("tribute", &object->tribute))
			SetDirty();
		if(ImGui::InputInt("threshold", &object->defenseThreshold))
			SetDirty();

		if(ImGui::TreeNode("fleets"))
		{
			index = 0;
			map<const Fleet *, int> modify;
			for(auto it = object->defenseFleets.begin(); it != object->defenseFleets.end();)
			{
				int count = 1;
				auto counter = it;
				while(next(counter) != object->defenseFleets.end()
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
							SetDirty();
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
					SetDirty();
				}

				++index;
				it += oldCount;
				ImGui::PopID();
			}

			// Now update any fleets entries.
			for(auto &&pair : modify)
			{
				auto first = find(object->defenseFleets.begin(), object->defenseFleets.end(), pair.first);
				if(pair.second <= 0)
					object->defenseFleets.erase(first, first + (-pair.second));
				else
					object->defenseFleets.insert(first, pair.second, pair.first);
			}

			// Now add the ability to add fleets.
			if(ImGui::BeginCombo("##fleets", ""))
			{
				for(const auto &item : GameData::Fleets())
					if(ImGui::Selectable(item.first.c_str()))
					{
						object->defenseFleets.emplace_back(&item.second);
						SetDirty();
					}
				ImGui::EndCombo();
			}

			ImGui::TreePop();
		}
		ImGui::TreePop();
	}


	static string landscapeName;
	static Sprite *landscape = nullptr;
	if(object->landscape)
		landscapeName = object->landscape->Name();
	if(ImGui::InputCombo("landscape", &landscapeName, &landscape, SpriteSet::GetSprites()))
	{
		object->landscape = landscape;
		GameData::Preload(object->landscape);
		landscape = nullptr;
		landscapeName.clear();
		SetDirty();
	}
	if(ImGui::InputText("music", &object->music, ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();

	if(ImGui::InputTextMultiline("description", &object->description, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();
	if(ImGui::InputTextMultiline("spaceport", &object->spaceport, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		SetDirty();

		if(object->spaceport.empty())
			object->attributes.erase("spaceport");
		else
			object->attributes.insert("spaceport");
	}

	{
		if(ImGui::BeginCombo("governments", object->government ? object->government->TrueName().c_str() : ""))
		{
			int index = 0;
			for(const auto &government : GameData::Governments())
			{
				const bool selected = &government.second == object->government;
				if(ImGui::Selectable(government.first.c_str(), selected))
				{
					object->government = &government.second;
					SetDirty();
				}
				++index;

				if(selected)
					ImGui::SetItemDefaultFocus();
			}

			if(ImGui::Selectable("[remove]"))
			{
				object->government = nullptr;
				SetDirty();
			}
			ImGui::EndCombo();
		}
	}

	if(ImGui::InputDoubleEx("required reputation", &object->requiredReputation))
		SetDirty();
	if(ImGui::InputDoubleEx("bribe", &object->bribe))
		SetDirty();
	if(ImGui::InputDoubleEx("security", &object->security))
		SetDirty();
	object->customSecurity = object->security != .25;

	object->inhabited = (!object->spaceport.empty() || object->requiredReputation || !object->defenseFleets.empty()) && !object->attributes.count("uninhabited");
}



void PlanetEditor::WriteToFile(DataWriter &writer, const Planet *planet)
{
	writer.Write("planet", planet->name);
	writer.BeginChild();

	if(!planet->attributes.empty() &&
			any_of(planet->attributes.begin(), planet->attributes.end(),
				[](const string &a) { return a != "spaceport" && a != "shipyard" && a != "outfitter"; }))
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
		writer.Write("government", planet->government->TrueName());
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
