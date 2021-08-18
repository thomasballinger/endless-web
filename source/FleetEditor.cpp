/* FleetEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "FleetEditor.h"

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



FleetEditor::FleetEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Fleet>(editor, show)
{
}



void FleetEditor::Render()
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

	if(ImGui::BeginCombo("fleet", object ? object->Name().c_str() : ""))
	{
		for(const auto &item : GameData::Fleets())
		{
			const bool selected = object ? object->Name() == item.first : false;
			if(ImGui::Selectable(item.first.c_str(), selected))
				object = const_cast<Fleet *>(&item.second);
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
				ImGui::SetTooltip("Select a fleet first.");
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
				ImGui::SetTooltip("Input the new name for the fleet above.");
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
				ImGui::SetTooltip("Select a fleet first.");
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
		*object = *GameData::defaultFleets.Get(object->fleetName);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Fleet *>(GameData::Fleets().Get(searchBox));
		*clone = *object;
		object = clone;

		object->fleetName = searchBox;
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderFleet();
	ImGui::End();
}



void FleetEditor::RenderFleet()
{
	ImGui::Text("name: %s", object->fleetName.c_str());

	string govName = object->government ? object->government->TrueName() : "";
	if(ImGui::BeginCombo("government", govName.c_str()))
	{
		for(const auto &item : GameData::Governments())
		{
			const bool selected = &item.second == object->government;
			if(ImGui::Selectable(item.first.c_str(), selected))
			{
				object->government = &item.second;
				SetDirty();
			}
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		if(ImGui::Selectable("[empty]"))
		{
			object->government = nullptr;
			SetDirty();
		}
		ImGui::EndCombo();
	}
	string names = object->names ? object->names->Name() : "";
	if(ImGui::BeginCombo("names", names.c_str()))
	{
		for(const auto &item : GameData::Phrases())
		{
			const bool selected = &item.second == object->names;
			if(ImGui::Selectable(item.first.c_str(), selected))
			{
				object->names = &item.second;
				SetDirty();
			}
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		if(ImGui::Selectable("[empty]"))
		{
			object->names = nullptr;
			SetDirty();
		}
		ImGui::EndCombo();
	}
	string fighterNames = object->fighterNames ? object->fighterNames->Name() : "";
	if(ImGui::BeginCombo("fighters", fighterNames.c_str()))
	{
		for(const auto &item : GameData::Phrases())
		{
			const bool selected = &item.second == object->fighterNames;
			if(ImGui::Selectable(item.first.c_str(), selected))
			{
				object->fighterNames = &item.second;
				SetDirty();
			}
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		if(ImGui::Selectable("[empty]"))
		{
			object->fighterNames = nullptr;
			SetDirty();
		}
		ImGui::EndCombo();
	}

	if(ImGui::InputInt("cargo", &object->cargo))
		SetDirty();
	if(ImGui::TreeNode("commodities"))
	{
		for(const auto &commodity : GameData::Commodities())
		{
			auto it = find(object->commodities.begin(), object->commodities.end(), commodity.name);
			bool has = it != object->commodities.end();
			if(ImGui::Checkbox(commodity.name.c_str(), &has))
			{
				if(!has)
					object->commodities.erase(it);
				else
					object->commodities.push_back(commodity.name);
				SetDirty();
			}
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("outfitters"))
	{
		auto found = object->outfitters.end();
		const Sale<Outfit> *toAdd = nullptr;
		int index = 0;
		for(auto it = object->outfitters.begin(); it != object->outfitters.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::BeginCombo("outfitter", (*it)->name.c_str()))
			{
				for(const auto &item : GameData::Outfitters())
				{
					const bool selected = &item.second == *it;
					if(ImGui::Selectable(item.first.c_str(), selected))
					{
						found = it;
						toAdd = &item.second;
						SetDirty();
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if(ImGui::Selectable("[remove]"))
			{
				found = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(found != object->outfitters.end())
		{
			object->outfitters.erase(found);
			if(toAdd)
				object->outfitters.insert(toAdd);
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("personality"))
	{
		if(ImGui::InputDoubleEx("confusion", &object->personality.confusionMultiplier))
			SetDirty();
		bool flag = object->personality.flags & Personality::PACIFIST;
		if(ImGui::Checkbox("pacifist", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::PACIFIST;
			else
				object->personality.flags &= ~Personality::PACIFIST;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FORBEARING;
		if(ImGui::Checkbox("forbearing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FORBEARING;
			else
				object->personality.flags &= ~Personality::FORBEARING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::TIMID;
		if(ImGui::Checkbox("timid", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::TIMID;
			else
				object->personality.flags &= ~Personality::TIMID;
			SetDirty();
		}
		flag = object->personality.flags & Personality::DISABLES;
		if(ImGui::Checkbox("disables", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::DISABLES;
			else
				object->personality.flags &= ~Personality::DISABLES;
			SetDirty();
		}
		flag = object->personality.flags & Personality::PLUNDERS;
		if(ImGui::Checkbox("plunders", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::PLUNDERS;
			else
				object->personality.flags &= ~Personality::PLUNDERS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::HEROIC;
		if(ImGui::Checkbox("heroic", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::HEROIC;
			else
				object->personality.flags &= ~Personality::HEROIC;
			SetDirty();
		}
		flag = object->personality.flags & Personality::STAYING;
		if(ImGui::Checkbox("staying", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::STAYING;
			else
				object->personality.flags &= ~Personality::STAYING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::ENTERING;
		if(ImGui::Checkbox("entering", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::ENTERING;
			else
				object->personality.flags &= ~Personality::ENTERING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::NEMESIS;
		if(ImGui::Checkbox("nemesis", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::NEMESIS;
			else
				object->personality.flags &= ~Personality::NEMESIS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::SURVEILLANCE;
		if(ImGui::Checkbox("surveillance", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::SURVEILLANCE;
			else
				object->personality.flags &= ~Personality::SURVEILLANCE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::UNINTERESTED;
		if(ImGui::Checkbox("uninterested", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::UNINTERESTED;
			else
				object->personality.flags &= ~Personality::UNINTERESTED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::WAITING;
		if(ImGui::Checkbox("waiting", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::WAITING;
			else
				object->personality.flags &= ~Personality::WAITING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::DERELICT;
		if(ImGui::Checkbox("derelict", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::DERELICT;
			else
				object->personality.flags &= ~Personality::DERELICT;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FLEEING;
		if(ImGui::Checkbox("fleeing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FLEEING;
			else
				object->personality.flags &= ~Personality::FLEEING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::ESCORT;
		if(ImGui::Checkbox("escort", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::ESCORT;
			else
				object->personality.flags &= ~Personality::ESCORT;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FRUGAL;
		if(ImGui::Checkbox("frugal", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FRUGAL;
			else
				object->personality.flags &= ~Personality::FRUGAL;
			SetDirty();
		}
		flag = object->personality.flags & Personality::COWARD;
		if(ImGui::Checkbox("coward", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::COWARD;
			else
				object->personality.flags &= ~Personality::COWARD;
			SetDirty();
		}
		flag = object->personality.flags & Personality::VINDICTIVE;
		if(ImGui::Checkbox("vindictive", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::VINDICTIVE;
			else
				object->personality.flags &= ~Personality::VINDICTIVE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::SWARMING;
		if(ImGui::Checkbox("swarming", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::SWARMING;
			else
				object->personality.flags &= ~Personality::SWARMING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::UNCONSTRAINED;
		if(ImGui::Checkbox("unconstrained", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::UNCONSTRAINED;
			else
				object->personality.flags &= ~Personality::UNCONSTRAINED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MINING;
		if(ImGui::Checkbox("mining", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MINING;
			else
				object->personality.flags &= ~Personality::MINING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::HARVESTS;
		if(ImGui::Checkbox("harvests", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::HARVESTS;
			else
				object->personality.flags &= ~Personality::HARVESTS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::APPEASING;
		if(ImGui::Checkbox("appeasing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::APPEASING;
			else
				object->personality.flags &= ~Personality::APPEASING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MUTE;
		if(ImGui::Checkbox("mute", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MUTE;
			else
				object->personality.flags &= ~Personality::MUTE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::OPPORTUNISTIC;
		if(ImGui::Checkbox("opportunistic", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::OPPORTUNISTIC;
			else
				object->personality.flags &= ~Personality::OPPORTUNISTIC;
			SetDirty();
		}
		flag = object->personality.flags & Personality::TARGET;
		if(ImGui::Checkbox("target", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::TARGET;
			else
				object->personality.flags &= ~Personality::TARGET;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MARKED;
		if(ImGui::Checkbox("marked", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MARKED;
			else
				object->personality.flags &= ~Personality::MARKED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::LAUNCHING;
		if(ImGui::Checkbox("launching", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::LAUNCHING;
			else
				object->personality.flags &= ~Personality::LAUNCHING;
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool variantsOpen = ImGui::TreeNode("variants");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Variant"))
		{
			Fleet::Variant var;
			var.weight = 1;
			object->variants.push_back(move(var));
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(variantsOpen)
	{
		int index = 0;
		auto found = object->variants.end();
		for(auto it = object->variants.begin(); it != object->variants.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("variant", "variant: %d", it->weight);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
				{
					found = it;
					SetDirty();
				}
				ImGui::EndPopup();
			}
			if(open)
			{
				if(ImGui::InputInt("weight", &it->weight))
					SetDirty();
				bool shipsOpen = ImGui::TreeNode("ships");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Add Ship"))
					{
						it->ships.push_back(nullptr);
						SetDirty();
					}
					ImGui::EndPopup();
				}
				if(shipsOpen)
				{
					auto found = it->ships.end();
					int modify = 0;
					auto modifyJt = it->ships.end();
					for(auto jt = it->ships.begin(); jt != it->ships.end(); ++jt)
					{
						ImGui::PushID(*jt ? (*jt)->VariantName().c_str() : "[empty]");
						auto first = jt;
						int count = 1;
						while(jt + 1 != it->ships.end() && *(jt + 1) == *jt)
						{
							++count;
							++jt;
						}
						string shipName = *first ? (*first)->VariantName() : "";
						bool shipOpen = ImGui::TreeNode("ship", "ship: %s %d", shipName.c_str(), count);
						if(ImGui::BeginPopupContextItem())
						{
							if(ImGui::Selectable("Remove"))
							{
								found = first;
								modify = -count;
								SetDirty();
							}
							ImGui::EndPopup();
						}
						if(shipOpen)
						{
							if(ImGui::InputText("ship##input", &shipName, ImGuiInputTextFlags_EnterReturnsTrue))
								if(GameData::Ships().Has(shipName))
								{
									*first = GameData::Ships().Get(shipName);
									SetDirty();
								}
							int oldCount = count;
							if(ImGui::InputInt("count", &count))
							{
								modify = count - oldCount;
								modifyJt = first;
								SetDirty();
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					if(found != it->ships.end())
						it->ships.erase(found);
					if(modifyJt != it->ships.end())
					{
						if(modify > 0)
							it->ships.insert(modifyJt, modify, *modifyJt);
						else if(modify < 0)
							it->ships.erase(modifyJt, modifyJt + (-modify));
						SetDirty();
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}



void FleetEditor::WriteToFile(DataWriter &writer, const Fleet *fleet)
{
	writer.Write("fleet", fleet->Name());
	writer.BeginChild();
	if(fleet->government)
		writer.Write("government", fleet->government->TrueName());
	if(fleet->names)
		writer.Write("names", fleet->names->Name());
	if(fleet->fighterNames)
		writer.Write("fighter", fleet->fighterNames->Name());
	if(fleet->cargo != 3)
		writer.Write("cargo", fleet->cargo);
	if(!fleet->commodities.empty())
	{
		writer.WriteToken("commodities");
		for(const auto &commodity : fleet->commodities)
			writer.WriteToken(commodity);
		writer.Write();
	}
	if(!fleet->outfitters.empty())
	{
		writer.WriteToken("outfitters");
		for(const auto &outfitter : fleet->outfitters)
			writer.WriteToken(outfitter->name);
		writer.Write();
	}
	if(fleet->personality.confusionMultiplier || fleet->personality.flags)
	{
		writer.Write("personality");
		writer.BeginChild();
		if(fleet->personality.confusionMultiplier != 10.)
			writer.Write("confusion", fleet->personality.confusionMultiplier);
		for(int i = 1; i <= (1 << 27); i <<= 1)
			switch(fleet->personality.flags & i)
			{
			case Personality::PACIFIST:
				writer.Write("pacifist");
				break;
			case Personality::FORBEARING:
				writer.Write("forebearing");
				break;
			case Personality::TIMID:
				writer.Write("timid");
				break;
			case Personality::DISABLES:
				writer.Write("disables");
				break;
			case Personality::PLUNDERS:
				writer.Write("plunders");
				break;
			case Personality::HEROIC:
				writer.Write("heroic");
				break;
			case Personality::STAYING:
				writer.Write("staying");
				break;
			case Personality::ENTERING:
				writer.Write("entering");
				break;
			case Personality::NEMESIS:
				writer.Write("nemesis");
				break;
			case Personality::SURVEILLANCE:
				writer.Write("surveillance");
				break;
			case Personality::UNINTERESTED:
				writer.Write("uninterested");
				break;
			case Personality::WAITING:
				writer.Write("waiting");
				break;
			case Personality::DERELICT:
				writer.Write("derelict");
				break;
			case Personality::FLEEING:
				writer.Write("fleeing");
				break;
			case Personality::ESCORT:
				writer.Write("escort");
				break;
			case Personality::FRUGAL:
				writer.Write("frugal");
				break;
			case Personality::COWARD:
				writer.Write("coward");
				break;
			case Personality::VINDICTIVE:
				writer.Write("vindictive");
				break;
			case Personality::SWARMING:
				writer.Write("swarming");
				break;
			case Personality::UNCONSTRAINED:
				writer.Write("unconstrained");
				break;
			case Personality::MINING:
				writer.Write("mining");
				break;
			case Personality::HARVESTS:
				writer.Write("harvests");
				break;
			case Personality::APPEASING:
				writer.Write("appeasing");
				break;
			case Personality::MUTE:
				writer.Write("mute");
				break;
			case Personality::OPPORTUNISTIC:
				writer.Write("opportunistic");
				break;
			case Personality::TARGET:
				writer.Write("target");
				break;
			case Personality::MARKED:
				writer.Write("marked");
				break;
			case Personality::LAUNCHING:
				writer.Write("launching");
				break;
			}
		writer.EndChild();
	}

	if(!fleet->variants.empty())
		for(const auto &variant : fleet->variants)
		{
			writer.WriteToken("variant");
			if(variant.weight > 1)
				writer.WriteToken(variant.weight);
			writer.Write();
			writer.BeginChild();
			for(auto it = variant.ships.begin(); it != variant.ships.end(); )
			{
				auto copy = it;
				int count = 1;
				while(copy + 1 != variant.ships.end() && *(copy + 1) == *copy)
				{
					++copy;
					++count;
				}
				writer.WriteToken((*it)->VariantName());
				if(count > 1)
					writer.WriteToken(count);
				writer.Write();
				it += count;
			}
			writer.EndChild();
		}
	writer.EndChild();
}
