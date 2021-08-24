/* OutfitterEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "OutfitterEditor.h"

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



OutfitterEditor::OutfitterEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Outfit>>(editor, show)
{
}



void OutfitterEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Outfitter Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginCombo("outfitter", object ? object->Name().c_str() : ""))
	{
		for(const auto &outfitter : GameData::Outfitters())
		{
			const bool selected = object ? object->Name() == outfitter.first : false;
			if(ImGui::Selectable(outfitter.first.c_str(), selected))
				object = const_cast<Sale<Outfit> *>(&outfitter.second);
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
				ImGui::SetTooltip("Select an outfitter first.");
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
				ImGui::SetTooltip("Input the new name for the outfitter above.");
			else if(!object)
				ImGui::SetTooltip("Select an outfitter first.");
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
				ImGui::SetTooltip("Select an outfitter first.");
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
		*object = *GameData::defaultOutfitSales.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Sale<Outfit> *>(GameData::Outfitters().Get(searchBox));
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
	RenderOutfitter();
	ImGui::End();
}



void OutfitterEditor::RenderOutfitter()
{
	ImGui::Text("outfitter: %s", object->name.c_str());
	int index = 0;
	const Outfit *toAdd = nullptr;
	const Outfit *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->Name();
		Outfit *change = nullptr;
		if(ImGui::InputCombo("outfit", &name, &change, GameData::Outfits()))
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

	static string outfitName;
	static Outfit *outfit = nullptr;
	if(ImGui::InputCombo("add outfit", &outfitName, &outfit, GameData::Outfits()))
	{
		object->insert(outfit);
		SetDirty();
		outfitName.clear();
	}
}



void OutfitterEditor::WriteToFile(DataWriter &writer, const Sale<Outfit> *outfitter)
{
	writer.Write("outfitter", outfitter->name);
	writer.BeginChild();
	for(auto it = object->begin(); it != object->end(); ++it)
		writer.Write((*it)->Name());
	writer.EndChild();
}
