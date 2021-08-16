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
#include "Imgui.h"
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
	: editor(editor), showShipMenu(show)
{
}



const list<Ship> &ShipEditor::Ships() const
{
	return ships;
}



const set<const Ship *> &ShipEditor::Dirty() const
{
	return dirty;
}



void ShipEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Ship Editor", &showShipMenu))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("ship", &searchBox);

	bool addEscort = ImGui::Button("Add as escort");
	ImGui::SameLine();
	bool reset = ImGui::Button("Reset");
	ImGui::SameLine();

	if(!editor.HasPlugin())
		ImGui::PushDisabled();
	bool save = ImGui::Button("Save");
	if(!editor.HasPlugin())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Load a plugin to save to a file.");
	}

	auto *ptr = GameData::Ships().Find(searchBox);
	if(!ptr)
	{
		ImGui::End();
		return;
	}
	if(!ship || ship->ModelName() != ptr->ModelName())
		ship.reset(new Ship(*ptr));

	if(addEscort)
	{
		editor.Player().ships.push_back(make_shared<Ship>(*ship.get()));
		editor.Player().ships.back()->SetName(searchBox);
		editor.Player().ships.back()->SetSystem(editor.Player().GetSystem());
		editor.Player().ships.back()->SetPlanet(editor.Player().GetPlanet());
		editor.Player().ships.back()->SetIsSpecial();
		editor.Player().ships.back()->SetIsYours();
		editor.Player().ships.back()->SetGovernment(GameData::PlayerGovernment());
	}
	if(reset)
		*ship = *GameData::defaultShips.Get(searchBox);

	// FIXME: There should be a better way to do this.
	Ship *overwrite = const_cast<Ship *>(GameData::Ships().Get(searchBox));
	*overwrite = *ship.get();

	if(save)
		WriteToPlugin(ship.get());

	ImGui::Separator();
	ImGui::Spacing();
	RenderShip();
	ImGui::End();
}



void ShipEditor::RenderShip()
{
	static string buffer;
	static double value;
	static bool bvalue;

	ImGui::InputText("name", &ship->name);
	ImGui::InputText("plural", &ship->pluralModelName);
	ImGui::InputText("noun", &ship->noun);
	RenderElement(ship.get());
	static string thumbnail;
	if(ship->thumbnail)
		thumbnail = ship->thumbnail->Name();
	if(ImGui::InputText("thumbnail", &thumbnail, ImGuiInputTextFlags_EnterReturnsTrue))
		ship->thumbnail = SpriteSet::Get(thumbnail);
	ImGui::Checkbox("never disabled", &ship->neverDisabled);
	bvalue = !ship->isCapturable;
	ImGui::Checkbox("uncapturable", &bvalue);
	ship->isCapturable = !bvalue;
	ImGui::InputInt("swizzle", &ship->customSwizzle);

	if(ImGui::TreeNode("attributes"))
	{
		if(ImGui::BeginCombo("category", ship->baseAttributes.Category().c_str()))
		{
			int index = 0;
			for(const auto &category : GameData::Category(CategoryType::SHIP))
			{
				const bool selected = ship->baseAttributes.Category().c_str() == category;
				if(ImGui::Selectable(category.c_str(), selected))
				{
					ship->baseAttributes.category = category;
					ship->attributes.category = category;
				}
				++index;

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::InputInt64Ex("cost", &ship->baseAttributes.cost);

		double oldMass = ship->baseAttributes.mass;
		ImGui::InputDoubleEx("mass", &ship->baseAttributes.mass);
		ship->attributes.mass -= oldMass;
		ship->attributes.mass += ship->baseAttributes.mass;

		for(auto &it : ship->baseAttributes.flareSprites)
			for(int i = 0; i < it.second; ++i)
				RenderElement(&it.first, "flare sprite");
		for(const auto &it : ship->baseAttributes.FlareSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("flare sound: %s", it.first->Name().c_str());
		for(auto &it : ship->baseAttributes.reverseFlareSprites)
			for(int i = 0; i < it.second; ++i)
				RenderElement(&it.first, "reverse flare sprite");
		for(const auto &it : ship->baseAttributes.ReverseFlareSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("reverse flare sound: %s", it.first->Name().c_str());
		for(auto &it : ship->baseAttributes.steeringFlareSprites)
			for(int i = 0; i < it.second; ++i)
				RenderElement(&it.first, "steering flare sprite");
		for(const auto &it : ship->baseAttributes.SteeringFlareSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("steering flare sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.AfterburnerEffects())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("afterburner effect: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.JumpEffects())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("jump effect: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.JumpSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("jump sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.JumpInSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("jump in sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.JumpOutSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("jump out sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.HyperSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("hyperdrive sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.HyperInSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("hyperdrive in sound: %s", it.first->Name().c_str());
		for(const auto &it : ship->baseAttributes.HyperOutSounds())
			for(int i = 0; i < it.second; ++i)
				ImGui::Text("hyperdrive out sound: %s", it.first->Name().c_str());
		for(auto &it : ship->baseAttributes.attributes)
			if(it.second)
			{
				value = it.second;
				if(ImGui::InputDoubleEx(it.first, &value, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					it.second = value;
					ship->attributes.attributes[it.first] = value;
				}
			}

		ImGui::Spacing();
		static string addAttribute;
		ImGui::InputText("add attribute name", &addAttribute);
		value = 0.;
		if(ImGui::InputDoubleEx("add attribute value", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			ship->baseAttributes.Set(addAttribute.c_str(), value);
			ship->attributes.Set(addAttribute.c_str(), value);
			addAttribute.clear();
		}

		ImGui::TreePop();
	}

	if(ImGui::TreeNode("outfits"))
	{
		for(auto &outfit : ship->outfits)
		{
			int amount = outfit.second;
			ImGui::InputInt(outfit.first->Name().c_str(), &amount);
			if(amount < 0)
				amount = 0;
			if(amount != outfit.second)
			{
				auto *var = outfit.first;
				ship->AddOutfit(var, -outfit.second);
				if(amount)
					ship->AddOutfit(var, amount);
				break;
			}
		}

		ImGui::Spacing();
		static string addOutfit;
		ImGui::InputText("add outfit", &addOutfit);
		int ivalue = 0;
		if(ImGui::InputInt("add amount", &ivalue, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			auto *outfit = GameData::Outfits().Find(addOutfit);
			ship->AddOutfit(outfit, ivalue);
			addOutfit.clear();
		}
		ImGui::TreePop();
	}

	// TODO: cargo.Save(out);
	ImGui::Text("crew: %d", ship->crew);
	ImGui::Text("fuel: %g", ship->fuel);
	ImGui::Text("shields: %g", ship->shields);
	ImGui::Text("hull: %g", ship->hull);
	ImGui::Text("position: %g %g", ship->position.X(), ship->position.Y());
		
	int index = 0;
	for(auto &point : ship->enginePoints)
	{
		ImGui::PushID(index++);
		if(ImGui::TreeNode("engine", "engine: %g %g", 2. * point.X(), 2. * point.Y()))
		{
			ImGui::InputDoubleEx("zoom", &point.zoom);
			value = point.facing.Degrees();
			ImGui::InputDoubleEx("angle", &value);
			point.facing = Angle(value);
			int ivalue = point.side;
			ImGui::Combo("under/over", &ivalue, "under\0over\0\0");
			point.side = !!ivalue;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	index = 0;
	for(auto &point : ship->reverseEnginePoints)
	{
		ImGui::PushID(index++);
		if(ImGui::TreeNode("reverse engine", "reverse engine: %g %g", 2. * point.X(), 2. * point.Y()))
		{
			ImGui::InputDoubleEx("zoom", &point.zoom);
			value = point.facing.Degrees() - 180.;
			ImGui::InputDoubleEx("angle", &value);
			point.facing = Angle(value) + 180.;
			int ivalue = point.side;
			ImGui::Combo("under/over", &ivalue, "under\0over\0\0");
			point.side = !!ivalue;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	index = 0;
	for(auto &point : ship->steeringEnginePoints)
	{
		ImGui::PushID(index++);
		if(ImGui::TreeNode("steering engine", "steering engine: %g %g", 2. * point.X(), 2. * point.Y()))
		{
			ImGui::InputDoubleEx("zoom", &point.zoom);
			value = point.facing.Degrees();
			ImGui::InputDoubleEx("angle", &value);
			point.facing = Angle(value);
			int ivalue = point.side;
			ImGui::Combo("under/over", &ivalue, "under\0over\0\0");
			point.side = !!ivalue;
			ImGui::Text(!point.steering ? "none" : point.steering == 1 ? "left" : "right");
			ivalue = point.steering;
			ImGui::Combo("none/left/right", &ivalue, "none\0left\0right\0\0");
			point.steering = ivalue;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	index = 0;
	for(Hardpoint &hardpoint : ship->armament.hardpoints)
	{
		ImGui::PushID(index++);
		const char *type = (hardpoint.IsTurret() ? "turret" : "gun");
		bool open;
		if(hardpoint.GetOutfit())
			open = ImGui::TreeNode("hardpoint", "%s: %g %g %s", type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y(),
				hardpoint.GetOutfit()->Name().c_str());
		else
			open = ImGui::TreeNode("hardpoint", "%s: %g %g", type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y());
		double hardpointAngle = hardpoint.GetBaseAngle().Degrees();
		if(open)
		{
			ImGui::InputDoubleEx("angle", &hardpointAngle);
			ImGui::Checkbox("parallel", &hardpoint.isParallel);
			int ivalue = hardpoint.isUnder;
			ImGui::Combo("under/over", &ivalue, "over\0under\0\0");
			hardpoint.isUnder = !!ivalue;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	index = 0;
	for(auto &bay : ship->bays)
	{
		ImGui::PushID(index++);
		double x = 2. * bay.point.X();
		double y = 2. * bay.point.Y();

		if(ImGui::TreeNode("bay: %s %g %g", bay.category.c_str(), x, y))
		{
			value = bay.facing.Degrees();
			ImGui::InputDoubleEx("angle", &value);
			bay.facing = Angle(value);

			int ivalue = bay.side;
			ImGui::Combo("bay side", &ivalue, "inside\0over\0under\0\0");
			bay.side = ivalue;

			static string launchEffect;
			for(size_t i = 0; i < bay.launchEffects.size(); ++i)
			{
				launchEffect = bay.launchEffects[i]->Name();
				if(ImGui::InputText("launch effect", &launchEffect, ImGuiInputTextFlags_EnterReturnsTrue))
					bay.launchEffects[i] = GameData::Effects().Get(move(launchEffect));
			}
		}
		ImGui::PopID();
	}
	for(const auto &leak : ship->leaks)
		ImGui::Text("leak: %s %d %d", leak.effect->Name().c_str(), leak.openPeriod, leak.closePeriod);
		
	for(const auto &it : ship->explosionEffects)
		if(it.second)
			ImGui::Text("explode: %s %d", it.first->Name().c_str(), it.second);
	for(const auto &it : ship->finalExplosions)
			if(it.second)
				ImGui::Text("final explode: %s %d", it.first->Name().c_str(), it.second);

	buffer.clear();
	if(ship->currentSystem)
		buffer = ship->currentSystem->Name();
	if(ImGui::InputText("system", &buffer))
		ship->currentSystem = GameData::Systems().Get(move(buffer));
	buffer.clear();
	if(ship->landingPlanet)
		buffer = ship->landingPlanet->TrueName();
	if(ImGui::InputText("planet", &buffer))
		ship->landingPlanet = GameData::Planets().Get(move(buffer));
	buffer.clear();
	if(ship->targetSystem)
		buffer = ship->targetSystem->Name();
	if(ImGui::InputText("destination system", &buffer))
		ship->targetSystem = GameData::Systems().Get(move(buffer));
	ImGui::Checkbox("parked", &ship->isParked);
}



void ShipEditor::RenderElement(Body *sprite, const std::string &name) const
{
	if(!sprite || !sprite->GetSprite())
		return;

	if(ImGui::TreeNode("sprite", "%s: %s", name.c_str(), sprite->GetSprite()->Name().c_str()))
	{
		double value = sprite->frameRate / 60.;
		if(ImGui::InputDoubleEx("frame rate", &value, ImGuiInputTextFlags_EnterReturnsTrue))
			sprite->frameRate = value * 60.;

		ImGui::InputInt("delay", &sprite->delay);
		ImGui::Checkbox("random start frame", &sprite->randomize);
		bool bvalue = !sprite->repeat;
		ImGui::Checkbox("no repeat", &bvalue);
		sprite->repeat = !bvalue;
		ImGui::Checkbox("rewind", &sprite->rewind);
		ImGui::TreePop();
	}
}



void ShipEditor::WriteToPlugin(const Ship *ship)
{
	if(!editor.HasPlugin())
		return;

	dirty.erase(ship);
	for(auto &&s : ships)
		if(s.name == ship->name)
		{
			s = *ship;
			return;
		}

	ships.push_back(*ship);
}



void ShipEditor::WriteToFile(DataWriter &writer, const Ship *ship)
{
	ship->Save(writer);
}



void ShipEditor::WriteAll()
{
	auto copy = dirty;
	for(auto &&s : copy)
		WriteToPlugin(s);
}
