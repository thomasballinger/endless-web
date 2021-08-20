/* ShipEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ShipEditor.h"

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



ShipEditor::ShipEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Ship>(editor, show)
{
}



void ShipEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Ship Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Add as Escort", nullptr, false, object))
			{
				editor.Player().ships.push_back(make_shared<Ship>(*object));
				editor.Player().ships.back()->SetName(searchBox);
				editor.Player().ships.back()->SetSystem(editor.Player().GetSystem());
				editor.Player().ships.back()->SetPlanet(editor.Player().GetPlanet());
				editor.Player().ships.back()->SetIsSpecial();
				editor.Player().ships.back()->SetIsYours();
				editor.Player().ships.back()->SetGovernment(GameData::PlayerGovernment());
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(ImGui::InputText("ship", &searchBox))
		if(auto *ptr = GameData::Ships().Find(searchBox))
		{
			object = const_cast<Ship *>(ptr);
			searchBox.clear();
		}
	if(!object || !IsDirty())
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!object || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a ship first.");
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
				ImGui::SetTooltip("Input the new name for the ship above.");
			else if(!object)
				ImGui::SetTooltip("Select a ship first.");
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
				ImGui::SetTooltip("Select a ship first.");
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
		*object = *GameData::defaultShips.Get(object->name);
		SetClean();
	} 
	if(clone)
	{
		auto *clone = const_cast<Ship *>(GameData::Ships().Get(searchBox));
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
	RenderShip();
	ImGui::End();
}



void ShipEditor::RenderShip()
{
	static string buffer;
	static double value;

	ImGui::Text("model: %s", object->modelName.c_str());
	if(ImGui::InputText("plural", &object->pluralModelName))
		SetDirty();
	if(ImGui::InputText("noun", &object->noun))
		SetDirty();
	RenderElement(object, "sprite");
	static string thumbnail;
	if(object->thumbnail)
		thumbnail = object->thumbnail->Name();
	if(ImGui::InputText("thumbnail", &thumbnail, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->thumbnail = SpriteSet::Get(thumbnail);
		SetDirty();
	}
	if(ImGui::Checkbox("never disabled", &object->neverDisabled))
		SetDirty();
	bool uncapturable = !object->isCapturable;
	if(ImGui::Checkbox("uncapturable", &uncapturable))
	{
		object->isCapturable = !uncapturable;
		SetDirty();
	}
	if(ImGui::InputInt("swizzle", &object->customSwizzle))
		SetDirty();

	if(ImGui::TreeNode("attributes"))
	{
		if(ImGui::BeginCombo("category", object->baseAttributes.Category().c_str()))
		{
			for(const auto &category : GameData::Category(CategoryType::SHIP))
			{
				const bool selected = object->baseAttributes.Category() == category;
				if(ImGui::Selectable(category.c_str(), selected))
				{
					object->baseAttributes.category = category;
					object->attributes.category = category;
					SetDirty();
				}

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if(ImGui::InputInt64Ex("cost", &object->baseAttributes.cost))
			SetDirty();

		double oldMass = object->baseAttributes.mass;
		if(ImGui::InputDoubleEx("mass", &object->baseAttributes.mass))
		{
			object->attributes.mass -= oldMass;
			object->attributes.mass += object->baseAttributes.mass;
			SetDirty();
		}

		bool openSprites = ImGui::TreeNode("sprites");
		if(ImGui::BeginPopupContextItem())
		{
			if(ImGui::Selectable("Add flare sprite"))
			{
				object->baseAttributes.flareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			if(ImGui::Selectable("Add reverse flare sprite"))
			{
				object->baseAttributes.reverseFlareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			if(ImGui::Selectable("Add steering flare sprite"))
			{
				object->baseAttributes.steeringFlareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			ImGui::EndPopup();
		}
		if(openSprites)
		{
			RenderSprites("flare sprite", object->baseAttributes.flareSprites);
			RenderSprites("reverse flare sprite", object->baseAttributes.reverseFlareSprites);
			RenderSprites("steering flare sprite", object->baseAttributes.steeringFlareSprites);
			ImGui::TreePop();
		}
		if(ImGui::TreeNode("sounds"))
		{
			RenderSound("flare sound", object->baseAttributes.flareSounds);
			RenderSound("reverse flare sound", object->baseAttributes.reverseFlareSounds);
			RenderSound("steering flare sound", object->baseAttributes.steeringFlareSounds);
			RenderSound("hyperdrive sound", object->baseAttributes.hyperSounds);
			RenderSound("hyperdrive in sound", object->baseAttributes.hyperInSounds);
			RenderSound("hyperdrive out sound", object->baseAttributes.hyperOutSounds);
			RenderSound("jump sound", object->baseAttributes.jumpSounds);
			RenderSound("jump in sound", object->baseAttributes.jumpInSounds);
			RenderSound("jump out sound", object->baseAttributes.jumpOutSounds);
			ImGui::TreePop();
		}
		if(ImGui::TreeNode("effects"))
		{
			RenderEffect("afterburner effect", object->baseAttributes.afterburnerEffects);
			RenderEffect("jump effect", object->baseAttributes.jumpEffects);
			ImGui::TreePop();
		}
		if(ImGui::InputDoubleEx("blast radius", &object->baseAttributes.blastRadius))
			SetDirty();
		if(ImGui::InputDoubleEx("shield damage", &object->baseAttributes.damage[Weapon::SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hull damage", &object->baseAttributes.damage[Weapon::HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hit force", &object->baseAttributes.damage[Weapon::HIT_FORCE]))
			SetDirty();
		for(auto &it : object->baseAttributes.attributes)
			if(it.second)
			{
				value = it.second;
				if(ImGui::InputDoubleEx(it.first, &value, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					it.second = value;
					object->attributes.attributes[it.first] = value;
					SetDirty();
				}
			}

		ImGui::Spacing();
		static string addAttribute;
		ImGui::InputText("add attribute name", &addAttribute);
		value = 0.;
		if(ImGui::InputDoubleEx("add attribute value", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->baseAttributes.Set(addAttribute.c_str(), value);
			object->attributes.Set(addAttribute.c_str(), value);
			addAttribute.clear();
			SetDirty();
		}

		ImGui::TreePop();
	}

	if(ImGui::TreeNode("outfits"))
	{
		for(auto &outfit : object->outfits)
		{
			int amount = outfit.second;
			ImGui::InputInt(outfit.first->Name().c_str(), &amount);
			if(amount < 0)
				amount = 0;
			if(amount != outfit.second)
			{
				auto *var = outfit.first;
				object->AddOutfit(var, -outfit.second);
				if(amount)
					object->AddOutfit(var, amount);
				SetDirty();
				break;
			}
		}

		ImGui::Spacing();
		static string addOutfit;
		ImGui::InputText("add outfit", &addOutfit);
		int ivalue = 0;
		if(ImGui::InputInt("add amount", &ivalue, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if(GameData::Outfits().Has(addOutfit))
			{
				auto *outfit = GameData::Outfits().Find(addOutfit);
				object->AddOutfit(outfit, ivalue);
				addOutfit.clear();
				SetDirty();
			}
			addOutfit.clear();
		}
		ImGui::TreePop();
	}

	if(ImGui::InputInt("crew", &object->crew))
		SetDirty();
	if(ImGui::InputDoubleEx("fuel", &object->fuel))
		SetDirty();
	if(ImGui::InputDoubleEx("shields", &object->shields))
		SetDirty();
	if(ImGui::InputDoubleEx("hull", &object->hull))
		SetDirty();

	int index = 0;
	bool engineOpen = ImGui::TreeNode("engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Engine"))
		{
			object->enginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(engineOpen)
	{
		auto found = object->enginePoints.end();
		for(auto it = object->enginePoints.begin(); it != object->enginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("engine", "engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("engine##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees();
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value);
				int ivalue = it->side;
				if(ImGui::Combo("under/over", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->enginePoints.end())
			object->enginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool reverseEngineOpen = ImGui::TreeNode("reverse engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Reverse Engine"))
		{
			object->reverseEnginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(reverseEngineOpen)
	{
		auto found = object->reverseEnginePoints.end();
		for(auto it = object->reverseEnginePoints.begin(); it != object->reverseEnginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("reverse engine", "reverse engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("reverse engine##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees() - 180.;
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value) + 180.;
				int ivalue = it->side;
				if(ImGui::Combo("under/over", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->reverseEnginePoints.end())
			object->reverseEnginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool steeringEngineOpen = ImGui::TreeNode("steering engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Steering Engine"))
		{
			object->steeringEnginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(steeringEngineOpen)
	{
		auto found = object->steeringEnginePoints.end();
		for(auto it = object->steeringEnginePoints.begin(); it != object->steeringEnginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("steering engine", "steering engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("steering engine##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees() - 180.;
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value) + 180.;
				int ivalue = it->side;
				if(ImGui::Combo("under/over", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;

				ivalue = it->steering;
				if(ImGui::Combo("none/left/right", &ivalue, "none\0left\0right\0\0"))
					SetDirty();
				it->steering = ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->steeringEnginePoints.end())
			object->steeringEnginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool hardpointsOpen = ImGui::TreeNode("hardpoints");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Turret"))
		{
			object->armament.hardpoints.emplace_back(Point(), Angle(), true, false, false);
			SetDirty();
		}
		if(ImGui::Selectable("Add Gun"))
		{
			object->armament.hardpoints.emplace_back(Point(), Angle(), false, true, false);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(hardpointsOpen)
	{
		auto found = object->armament.hardpoints.end();
		for(auto it = object->armament.hardpoints.begin(); it != object->armament.hardpoints.end(); ++it)
		{
			ImGui::PushID(index++);
			const char *type = (it->IsTurret() ? "turret" : "gun");
			bool open = it->GetOutfit()
				? ImGui::TreeNode("hardpoint", "%s: %g %g %s", type, 2. * it->GetPoint().X(), 2. * it->GetPoint().Y(), it->GetOutfit()->Name().c_str())
				: ImGui::TreeNode("hardpoint", "%s: %g %g", type, 2. * it->GetPoint().X(), 2. * it->GetPoint().Y());
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
				{
					found = it;
					SetDirty();
				}
				ImGui::EndPopup();
			}

			double hardpointAngle = it->GetBaseAngle().Degrees();
			if(open)
			{
				int ivalue = it->isTurret;
				if(ImGui::Combo("category", &ivalue, "gun\0turret\0\0"))
					SetDirty();
				it->isTurret = !!ivalue;
				double pos[2] = {2. * it->point.X(), 2. * it->point.Y()};
				if(ImGui::InputDouble2Ex("hardpoint##input", pos))
				{
					it->point.Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				string outfitName = it->GetOutfit() ? it->GetOutfit()->Name() : "";
				if(ImGui::InputText("outfit", &outfitName, ImGuiInputTextFlags_EnterReturnsTrue))
					if(GameData::Outfits().Has(outfitName))
					{
						it->outfit = GameData::Outfits().Get(outfitName);
						SetDirty();
					}
				if(!it->IsTurret() && ImGui::InputDoubleEx("angle", &hardpointAngle))
					SetDirty();
				if(!it->IsTurret() && ImGui::Checkbox("parallel", &it->isParallel))
					SetDirty();
				ivalue = it->isUnder;
				if(ImGui::Combo("under/over", &ivalue, "over\0under\0\0"))
					SetDirty();
				it->isUnder = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->armament.hardpoints.end())
			object->armament.hardpoints.erase(found);
		ImGui::TreePop();
	}
	index = 0;
	bool baysOpen = ImGui::TreeNode("bays");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Bay"))
		{
			object->bays.emplace_back(0., 0., GameData::Category(CategoryType::BAY)[0]);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(baysOpen)
	{
		auto found = object->bays.end();
		for(auto it = object->bays.begin(); it != object->bays.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("bay: %s %g %g", it->category.c_str(), 2. * it->point.X(), 2. * it->point.Y());
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
				if(ImGui::BeginCombo("category", it->category.c_str()))
				{
					for(const auto &category : GameData::Category(CategoryType::BAY))
					{
						const bool selected = it->category == category;
						if(ImGui::Selectable(category.c_str(), selected))
						{
							it->category = category;
							SetDirty();
						}

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				double pos[2] = {2. * it->point.X(), 2. * it->point.Y()};
				if(ImGui::InputDouble2Ex("pos", pos))
				{
					it->point.Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				value = it->facing.Degrees();
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value);

				int ivalue = it->side;
				if(ImGui::Combo("bay side", &ivalue, "inside\0over\0under\0\0"))
					SetDirty();
				it->side = ivalue;

				static std::string effectName;
				effectName.clear();
				bool effectOpen = ImGui::TreeNode("launch effects");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Add Effect"))
					{
						it->launchEffects.emplace_back();
						SetDirty();
					}
					ImGui::EndPopup();
				}
				if(effectOpen)
				{
					const Effect *toAdd = nullptr;
					auto toRemove = it->launchEffects.end();
					int index = 0;
					for(auto jt = it->launchEffects.begin(); jt != it->launchEffects.end(); ++jt)
					{
						ImGui::PushID(index++);

						effectName = *jt ? (*jt)->Name() : "";
						bool effectItselfOpen = ImGui::TreeNode("effect##node", "effect: %s", effectName.c_str());
						if(ImGui::BeginPopupContextItem())
						{
							if(ImGui::Selectable("Remove"))
							{
								toRemove = jt;
								SetDirty();
							}
							ImGui::EndPopup();
						}
						if(effectItselfOpen)
						{
							if(ImGui::InputText("effect", &effectName, ImGuiInputTextFlags_EnterReturnsTrue))
								if(GameData::Effects().Has(effectName))
								{
									toAdd = GameData::Effects().Get(effectName);
									toRemove = jt;
									SetDirty();
								}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}

					if(toAdd)
						it->launchEffects.push_back(toAdd);
					if(toRemove != it->launchEffects.end())
						it->launchEffects.erase(toRemove);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->bays.end())
			object->bays.erase(found);
		ImGui::TreePop();
	}

	bool leaksOpen = ImGui::TreeNode("leaks");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Leak"))
		{
			object->leaks.emplace_back();
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(leaksOpen)
	{
		index = 0;
		auto found = object->leaks.end();
		for(auto it = object->leaks.begin(); it != object->leaks.end(); ++it)
		{
			ImGui::PushID(index++);
			static string effectName;
			effectName = it->effect ? it->effect->Name() : "";
			bool open = ImGui::TreeNode("leak", "leak: %s %d %d", effectName.c_str(), it->openPeriod, it->closePeriod);
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
				if(ImGui::InputText("leak##input", &effectName, ImGuiInputTextFlags_EnterReturnsTrue))
					if(GameData::Effects().Has(effectName))
					{
						it->effect = GameData::Effects().Get(effectName);
						SetDirty();
					}
				if(ImGui::InputInt("open period", &it->openPeriod))
					SetDirty();
				if(ImGui::InputInt("close period", &it->closePeriod))
					SetDirty();
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->leaks.end())
			object->leaks.erase(found);
		ImGui::TreePop();
	}

	RenderEffect("explode", object->explosionEffects);
	RenderEffect("final explode", object->finalExplosions);

	if(ImGui::InputTextMultiline("description", &object->description, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();
}



void ShipEditor::WriteToFile(DataWriter &writer, const Ship *ship)
{
	writer.Write("ship", ship->modelName);
	writer.BeginChild();
	if(ship->pluralModelName != ship->modelName + 's')
		writer.Write("plural", ship->pluralModelName);
	if(!ship->noun.empty())
		writer.Write("noun", ship->noun);
	ship->SaveSprite(writer);
	if(ship->thumbnail)
		writer.Write("thumbnail", ship->thumbnail->Name());

	if(ship->neverDisabled)
		writer.Write("never disabled");
	if(!ship->isCapturable)
		writer.Write("uncapturable");
	if(ship->customSwizzle >= 0)
		writer.Write("swizzle", ship->customSwizzle);

	writer.Write("attributes");
	writer.BeginChild();
	writer.Write("category", ship->baseAttributes.Category());
	writer.Write("cost", ship->baseAttributes.Cost());
	writer.Write("mass", ship->baseAttributes.Mass());

	if(!ship->baseAttributes.Licenses().empty())
	{
		writer.WriteToken("licenses");
		for(auto &&license : ship->baseAttributes.Licenses())
			writer.WriteToken(license);
		writer.Write();
	}

	for(const auto &it : ship->baseAttributes.FlareSprites())
		for(int i = 0; i < it.second; ++i)
			it.first.SaveSprite(writer, "flare sprite");
	for(const auto &it : ship->baseAttributes.FlareSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("flare sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.ReverseFlareSprites())
		for(int i = 0; i < it.second; ++i)
			it.first.SaveSprite(writer, "reverse flare sprite");
	for(const auto &it : ship->baseAttributes.ReverseFlareSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("reverse flare sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.SteeringFlareSprites())
		for(int i = 0; i < it.second; ++i)
			it.first.SaveSprite(writer, "steering flare sprite");
	for(const auto &it : ship->baseAttributes.SteeringFlareSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("steering flare sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.AfterburnerEffects())
		for(int i = 0; i < it.second; ++i)
			writer.Write("afterburner effect", it.first->Name());
	for(const auto &it : ship->baseAttributes.JumpEffects())
		for(int i = 0; i < it.second; ++i)
			writer.Write("jump effect", it.first->Name());
	for(const auto &it : ship->baseAttributes.JumpSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("jump sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.JumpInSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("jump in sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.JumpOutSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("jump writer sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.HyperSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("hyperdrive sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.HyperInSounds())
		for(int i = 0; i < it.second; ++i)
			writer.Write("hyperdrive in sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.HyperOutSounds())
		for(int i = 0; i < it.second; ++i)
		writer.Write("hyperdrive writer sound", it.first->Name());
	for(const auto &it : ship->baseAttributes.Attributes())
	{
		if(it.first == string("gun ports") || it.first == string("turret mounts"))
			continue;
		if(it.second)
			writer.Write(it.first, it.second);
	}
	if(ship->baseAttributes.BlastRadius()
			|| ship->baseAttributes.ShieldDamage()
			|| ship->baseAttributes.HullDamage()
			|| ship->baseAttributes.HitForce())
	{
		writer.Write("weapon");
		writer.BeginChild();
		if(ship->baseAttributes.BlastRadius())
			writer.Write("blast radius", ship->baseAttributes.BlastRadius());
		if(ship->baseAttributes.ShieldDamage())
			writer.Write("shield damage", ship->baseAttributes.ShieldDamage());
		if(ship->baseAttributes.HullDamage())
			writer.Write("hull damage", ship->baseAttributes.HullDamage());
		if(ship->baseAttributes.HitForce())
			writer.Write("hit force", ship->baseAttributes.HitForce());
		writer.EndChild();
	}
	writer.EndChild();

	writer.Write("outfits");
	writer.BeginChild();
	{
		using OutfitElement = pair<const Outfit *const, int>;
		WriteSorted(ship->outfits,
			[](const OutfitElement *lhs, const OutfitElement *rhs)
				{ return lhs->first->Name() < rhs->first->Name(); },
			[&writer](const OutfitElement &it){
				if(it.second == 1)
					writer.Write(it.first->Name());
				else
					writer.Write(it.first->Name(), it.second);
			});
	}
	writer.EndChild();

	for(const auto &point : ship->enginePoints)
	{
		writer.Write("engine", 2. * point.X(), 2. * point.Y());
		writer.BeginChild();
		if(point.zoom != 1.)
			writer.Write("zoom", point.zoom);
		if(point.facing.Degrees())
			writer.Write("angle", point.facing.Degrees());
		if(point.side)
			writer.Write("over");
		writer.EndChild();
	}
	for(const auto &point : ship->reverseEnginePoints)
	{
		writer.Write("reverse engine", 2. * point.X(), 2. * point.Y());
		writer.BeginChild();
		if(point.zoom != 1.)
			writer.Write("zoom", point.zoom);
		if(point.facing.Degrees())
			writer.Write("angle", point.facing.Degrees() - 180.);
		if(point.side)
			writer.Write("over");
		writer.EndChild();
	}
	for(const auto &point : ship->steeringEnginePoints)
	{
		writer.Write("steering engine", 2. * point.X(), 2. * point.Y());
		writer.BeginChild();
		if(point.zoom != 1.)
			writer.Write("zoom", point.zoom);
		if(point.facing.Degrees())
			writer.Write("angle", point.facing.Degrees());
		if(point.side)
			writer.Write("over");
		if(point.steering)
			writer.Write(point.steering == 1 ? "left" : "right");
		writer.EndChild();
	}
	for(const Hardpoint &hardpoint : ship->armament.Get())
	{
		const char *type = (hardpoint.IsTurret() ? "turret" : "gun");
		if(hardpoint.GetOutfit())
			writer.Write(type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y(),
				hardpoint.GetOutfit()->Name());
		else
			writer.Write(type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y());
		double hardpointAngle = hardpoint.GetBaseAngle().Degrees();
		writer.BeginChild();
		{
			if(hardpointAngle)
				writer.Write("angle", hardpointAngle);
			if(hardpoint.IsParallel())
				writer.Write("parallel");
			if(hardpoint.IsUnder() && hardpoint.IsTurret())
				writer.Write("under");
			else if(!hardpoint.IsUnder() && !hardpoint.IsTurret())
				writer.Write("over");
		}
		writer.EndChild();
	}
	for(const auto &bay : ship->bays)
	{
		double x = 2. * bay.point.X();
		double y = 2. * bay.point.Y();

		writer.Write("bay", bay.category, x, y);
		if(!bay.launchEffects.empty() || bay.facing.Degrees() || bay.side)
		{
			writer.BeginChild();
			{
				if(bay.facing.Degrees())
					writer.Write("angle", bay.facing.Degrees());
				if(bay.side)
					writer.Write(!bay.side ? "inside" : bay.side == 1 ? "over" : "under");
				for(const Effect *effect : bay.launchEffects)
					if(effect->Name() != "basic launch")
						writer.Write("launch effect", effect->Name());
			}
			writer.EndChild();
		}
	}
	for(const auto &leak : ship->leaks)
		writer.Write("leak", leak.effect->Name(), leak.openPeriod, leak.closePeriod);

	using EffectElement = pair<const Effect *const, int>;
	auto effectSort = [](const EffectElement *lhs, const EffectElement *rhs)
		{ return lhs->first->Name() < rhs->first->Name(); };
	WriteSorted(ship->explosionEffects, effectSort, [&writer](const EffectElement &it)
	{
		if(it.second)
			writer.Write("explode", it.first->Name(), it.second);
	});
	WriteSorted(ship->finalExplosions, effectSort, [&writer](const EffectElement &it)
	{
		if(it.second)
			writer.Write("final explode", it.first->Name(), it.second);
	});
	if(!ship->description.empty())
	{
		size_t newline = ship->description.find('\n');
		size_t start = 0;
		do {
			writer.Write("description", ship->description.substr(start, newline - start));
			start = newline + 1;
			newline = ship->description.find('\n', start);
		} while(newline != string::npos);
	}

	writer.EndChild();
}
