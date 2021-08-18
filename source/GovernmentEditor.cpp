/* GovernmentEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "GovernmentEditor.h"

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



GovernmentEditor::GovernmentEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Government>(editor, show)
{
}



void GovernmentEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Government Editor", &show))
	{
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginCombo("government", object ? object->Name().c_str() : ""))
	{
		for(const auto &gov : GameData::Governments())
		{
			const bool selected = object ? object->Name() == gov.first : false;
			if(ImGui::Selectable(gov.first.c_str(), selected))
				object = const_cast<Government *>(&gov.second);
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
				ImGui::SetTooltip("Select a government first.");
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
				ImGui::SetTooltip("Input the new name for the government above.");
			else if(!object)
				ImGui::SetTooltip("Select a hazard first.");
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
				ImGui::SetTooltip("Select a government first.");
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
		*object = *GameData::defaultGovernments.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Government *>(GameData::Governments().Get(searchBox));
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
	RenderGovernment();
	ImGui::End();
}



void GovernmentEditor::RenderGovernment()
{
	if(ImGui::InputText("display name", &object->displayName))
		SetDirty();
	if(ImGui::InputInt("swizzle", &object->swizzle))
		SetDirty();
	float color[3] = {};
	color[0] = object->color.Get()[0];
	color[1] = object->color.Get()[1];
	color[2] = object->color.Get()[2];
	if(ImGui::ColorEdit3("color", color))
	{
		object->color = Color(color[0], color[1], color[2]);
		SetDirty();
	}
	if(ImGui::InputDoubleEx("player reputation", &object->initialPlayerReputation))
		SetDirty();
	if(ImGui::InputDoubleEx("crew attack", &object->crewAttack))
	{
		object->crewAttack = max(0., object->crewAttack);
		SetDirty();
	}
	if(ImGui::InputDoubleEx("crew defense", &object->crewDefense))
	{
		object->crewDefense = max(0., object->crewDefense);
		SetDirty();
	}

	if(ImGui::TreeNode("attitude towards"))
	{
		auto toRemove = object->attitudeToward.end();
		const Government *toAdd = nullptr;
		int index = 0;
		for(auto it = object->attitudeToward.begin(); it != object->attitudeToward.end(); ++it)
		{
			ImGui::PushID(index++);
			string govName = it->first ? it->first->TrueName() : "";
			if(ImGui::BeginCombo("##gov", govName.c_str()))
			{
				for(const auto &gov : GameData::Governments())
				{
					const bool selected = it->first == &gov.second;
					if(ImGui::Selectable(gov.second.TrueName().c_str(), selected))
					{
						toAdd = &gov.second;
						toRemove = it;
						SetDirty();
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			if(ImGui::InputDoubleEx("##count", &it->second, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!it->second)
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toRemove != object->attitudeToward.end())
		{
			double value = toRemove->second;
			object->attitudeToward.erase(toRemove);
			if(toAdd)
				object->attitudeToward[toAdd] = value;
		}

		ImGui::Spacing();
		if(ImGui::BeginCombo("add government", ""))
		{
			for(const auto &gov : GameData::Governments())
			{
				if(object->attitudeToward.find(&gov.second) != object->attitudeToward.end())
					continue;
				if(ImGui::Selectable(gov.second.TrueName().c_str()))
				{
					object->attitudeToward.emplace(&gov.second, 0.);
					SetDirty();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("penalty for"))
	{
		if(ImGui::InputDoubleEx("assist", &object->penaltyFor[ShipEvent::ASSIST]))
			SetDirty();
		if(ImGui::InputDoubleEx("disable", &object->penaltyFor[ShipEvent::DISABLE]))
			SetDirty();
		if(ImGui::InputDoubleEx("board", &object->penaltyFor[ShipEvent::BOARD]))
			SetDirty();
		if(ImGui::InputDoubleEx("capture", &object->penaltyFor[ShipEvent::CAPTURE]))
			SetDirty();
		if(ImGui::InputDoubleEx("destroy", &object->penaltyFor[ShipEvent::DESTROY]))
			SetDirty();
		if(ImGui::InputDoubleEx("atrocity", &object->penaltyFor[ShipEvent::ATROCITY]))
			SetDirty();
		ImGui::TreePop();
	}

	if(ImGui::InputDoubleEx("bribe", &object->bribe))
		SetDirty();
	if(ImGui::InputDoubleEx("fine", &object->fine))
		SetDirty();
	string deathSentence = object->deathSentence ? object->deathSentence->Name() : "";
	if(ImGui::InputText("death sentence", &deathSentence, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Conversations().Has(deathSentence))
		{
			object->deathSentence = GameData::Conversations().Get(deathSentence);
			SetDirty();
		}
	string friendlyHail = object->friendlyHail ? object->friendlyHail->Name() : "";
	if(ImGui::InputText("friendly hail", &friendlyHail, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Phrases().Has(friendlyHail))
		{
			object->friendlyHail = GameData::Phrases().Get(friendlyHail);
			SetDirty();
		}
	string friendlyDisabledHail = object->friendlyDisabledHail ? object->friendlyDisabledHail->Name() : "";
	if(ImGui::InputText("friendly disabled hail", &friendlyDisabledHail, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Phrases().Has(friendlyDisabledHail))
		{
			object->friendlyDisabledHail = GameData::Phrases().Get(friendlyDisabledHail);
			SetDirty();
		}
	string hostileHail = object->hostileHail ? object->hostileHail->Name() : "";
	if(ImGui::InputText("hostile hail", &hostileHail, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Phrases().Has(hostileHail))
		{
			object->hostileHail = GameData::Phrases().Get(hostileHail);
			SetDirty();
		}
	string hostileDisabledHail = object->hostileDisabledHail? object->hostileDisabledHail->Name() : "";
	if(ImGui::InputText("hostile disabled hail", &hostileDisabledHail, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Phrases().Has(hostileDisabledHail))
		{
			object->hostileDisabledHail = GameData::Phrases().Get(hostileDisabledHail);
			SetDirty();
		}

	if(ImGui::InputText("language", &object->language))
		SetDirty();
	string raidName = object->raidFleet ? object->raidFleet->Name() : "";
	if(ImGui::InputText("raid fleet", &raidName, ImGuiInputTextFlags_EnterReturnsTrue))
		if(GameData::Fleets().Has(raidName))
		{
			object->raidFleet = GameData::Fleets().Get(raidName);
			SetDirty();
		}

	static string enforcements;
	if(ImGui::InputTextMultiline("enforces", &enforcements))
		SetDirty();
}



void GovernmentEditor::WriteToFile(DataWriter &writer, const Government *government)
{
	writer.Write("government", government->TrueName());
	writer.BeginChild();
	if(government->displayName != government->name)
		writer.Write("display name", government->displayName);
	if(government->swizzle)
		writer.Write("swizzle", government->swizzle);
	writer.Write("color", government->color.Get()[0], government->color.Get()[1], government->color.Get()[2]);
	if(government->initialPlayerReputation)
		writer.Write("player reputation", government->initialPlayerReputation);
	if(government->crewAttack != 1.)
		writer.Write("crew attack", government->crewAttack);
	if(government->crewDefense != 1.)
		writer.Write("crew defense", government->crewDefense);
	if(!government->attitudeToward.empty())
	{
		writer.Write("attitude toward");
		writer.BeginChild();
		for(auto &&pair : government->attitudeToward)
			if(pair.second)
				writer.Write(pair.first->TrueName(), pair.second);
		writer.EndChild();
	}

	if(government->penaltyFor.at(ShipEvent::ASSIST) != -.1
			|| government->penaltyFor.at(ShipEvent::DISABLE) != .5
			|| government->penaltyFor.at(ShipEvent::BOARD) != .3
			|| government->penaltyFor.at(ShipEvent::CAPTURE) != 1.
			|| government->penaltyFor.at(ShipEvent::DESTROY) != 1.
			|| government->penaltyFor.at(ShipEvent::ATROCITY) != 10.)
	{
		writer.Write("penalty for");
		if(government->penaltyFor.at(ShipEvent::ASSIST) != -.1)
			writer.Write("assist", government->penaltyFor.at(ShipEvent::ASSIST));
		if(government->penaltyFor.at(ShipEvent::DISABLE) != .5)
			writer.Write("disable", government->penaltyFor.at(ShipEvent::DISABLE));
		if(government->penaltyFor.at(ShipEvent::BOARD) != .3)
			writer.Write("board", government->penaltyFor.at(ShipEvent::BOARD));
		if(government->penaltyFor.at(ShipEvent::CAPTURE) != 1.)
			writer.Write("capture", government->penaltyFor.at(ShipEvent::CAPTURE));
		if(government->penaltyFor.at(ShipEvent::DESTROY) != 1.)
			writer.Write("destroy", government->penaltyFor.at(ShipEvent::DESTROY));
		if(government->penaltyFor.at(ShipEvent::ATROCITY) != 10.)
			writer.Write("atrocity", government->penaltyFor.at(ShipEvent::ATROCITY));
	}

	if(government->bribe)
		writer.Write("bribe", government->bribe);
	if(government->fine != 1.)
		writer.Write("fine", government->fine);
	if(government->deathSentence)
		writer.Write("death sentence", government->deathSentence->Name());
	if(government->friendlyHail)
		writer.Write("friendly hail", government->friendlyHail->Name());
	if(government->friendlyDisabledHail && government->friendlyDisabledHail->Name() != "friendly disabled")
		writer.Write("friendly disabled hail", government->friendlyDisabledHail->Name());
	if(government->hostileHail)
		writer.Write("hostile hail", government->hostileHail->Name());
	if(government->hostileDisabledHail && government->hostileDisabledHail->Name() != "hostile disabled")
		writer.Write("hostile disabled hail", government->hostileDisabledHail->Name());
	if(!government->language.empty())
		writer.Write("language", government->language);
	if(government->raidFleet)
		writer.Write("raid", government->raidFleet->Name());
	if(!government->enforcementZones.empty())
	{
		writer.Write("enforces");
		for(auto &&filter : government->enforcementZones)
			filter.Save(writer);
	}
	writer.EndChild();
}
