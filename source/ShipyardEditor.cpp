/* ShipyardEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ShipyardEditor.h"

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
#include "ShipEvent.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



ShipyardEditor::ShipyardEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Ship>>(editor, show)
{
}



void ShipyardEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Shipyard Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginCombo("shipyard", object ? object->Name().c_str() : ""))
	{
		for(const auto &shipyard : GameData::Shipyards())
		{
			const bool selected = object ? object->Name() == shipyard.first : false;
			if(ImGui::Selectable(shipyard.first.c_str(), selected))
				object = const_cast<Sale<Ship> *>(&shipyard.second);
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::InputText("new clone name:", &searchBox);

	if(!object || !IsDirty())
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!object || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a shipyard first.");
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
				ImGui::SetTooltip("Input the new name for the shipyard above.");
			else if(!object)
				ImGui::SetTooltip("Select an shipyard first.");
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
				ImGui::SetTooltip("Select a shipyard first.");
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
		*object = *GameData::defaultShipSales.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Sale<Ship> *>(GameData::Shipyards().Get(searchBox));
		*clone = *object;
		object = clone;

		object->name = searchBox;
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderShipyard();
	ImGui::End();
}



void ShipyardEditor::RenderShipyard()
{
	ImGui::Text("shipyard: %s", object->name.c_str());
	int index = 0;
	const Ship *toAdd = nullptr;
	const Ship *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->ModelName();
		Ship *change = nullptr;
		if(ImGui::InputCombo("ship", &name, &change, GameData::Ships()))
		{
			if(name.empty())
			{
				toRemove = *it;
				SetDirty();
			}
			else
			{
				toAdd = change;
				toRemove = *it;
				SetDirty();
			}
		}
		ImGui::PopID();
	}
	if(toAdd)
		object->insert(toAdd);
	if(toRemove)
		object->erase(toRemove);

	static string shipName;
	static Ship *ship = nullptr;
	if(ImGui::InputCombo("add ship", &shipName, &ship, GameData::Ships()))
	{
		object->insert(ship);
		SetDirty();
		shipName.clear();
	}
}



void ShipyardEditor::WriteToFile(DataWriter &writer, const Sale<Ship> *shipyard)
{
	writer.Write("shipyard", shipyard->name);
	writer.BeginChild();
	for(auto it = object->begin(); it != object->end(); ++it)
		writer.Write((*it)->ModelName());
	writer.EndChild();
}
