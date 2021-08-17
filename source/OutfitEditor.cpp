/* OutfitEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "OutfitEditor.h"

#include "Audio.h"
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
#include "Weapon.h"

#include <cassert>
#include <map>

using namespace std;



OutfitEditor::OutfitEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Outfit>(editor, show)
{
}



void OutfitEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Outfit Editor", &show))
	{
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::InputText("outfit", &searchBox))
	{
		if(auto *ptr = GameData::Outfits().Find(searchBox))
		{
			object = const_cast<Outfit *>(ptr);
			searchBox.clear();
		}
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
				ImGui::SetTooltip("Select an outfit first.");
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
				ImGui::SetTooltip("Input the new name for the outfit above.");
			else if(!object)
				ImGui::SetTooltip("Select a outfit first.");
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
				ImGui::SetTooltip("Select a outfit first.");
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
		*object = *GameData::defaultOutfits.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Outfit *>(GameData::Outfits().Get(searchBox));
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
	RenderOutfit();
	ImGui::End();
}



void OutfitEditor::RenderOutfit()
{
	static string str;
	ImGui::Text("outfit: %s", object->Name().c_str());
	if(ImGui::BeginCombo("category", object->Category().c_str()))
	{
		int index = 0;
		for(const auto &category : GameData::Category(CategoryType::OUTFIT))
		{
			const bool selected = object->Category().c_str() == category;
			if(ImGui::Selectable(category.c_str(), selected))
			{
				object->category = category;
				SetDirty();
			}
			++index;

			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if(ImGui::InputText("plural", &object->pluralName))
		SetDirty();
	if(ImGui::InputInt64Ex("cost", &object->cost))
		SetDirty();
	if(ImGui::InputDoubleEx("mass", &object->mass))
		SetDirty();

	str.clear();
	if(object->flotsamSprite)
		str = object->flotsamSprite->Name();
	if(ImGui::InputText("floatsam sprite", &str, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->flotsamSprite = SpriteSet::Get(str);
		SetDirty();
	}

	str.clear();
	if(object->thumbnail)
		str = object->thumbnail->Name();
	if(ImGui::InputText("thumbnail", &str, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->thumbnail = SpriteSet::Get(str);
		SetDirty();
	}

	if(ImGui::InputTextMultiline("description", &object->description, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();

	int index = 0;
	if(ImGui::TreeNode("licenses"))
	{
		auto toRemove = object->licenses.end();
		for(auto it = object->licenses.begin(); it != object->licenses.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::InputText("", &*it, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(it->empty())
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toRemove != object->licenses.end())
			object->licenses.erase(toRemove);
		ImGui::Spacing();

		static string addLicense;
		if(ImGui::InputText("##license", &addLicense, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->licenses.push_back(move(addLicense));
			SetDirty();
		}
		ImGui::TreePop();

	}

	if(ImGui::TreeNode("attributes"))
	{
		for(auto &it : object->attributes)
			if(it.second)
				if(ImGui::InputDoubleEx(it.first, &it.second, ImGuiInputTextFlags_EnterReturnsTrue))
					SetDirty();

		ImGui::Spacing();
		static string addAttribute;
		ImGui::InputText("add attribute name", &addAttribute);
		static double value = 0.;
		if(ImGui::InputDoubleEx("add attribute value", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->Set(addAttribute.c_str(), value);
			object->Set(addAttribute.c_str(), value);
			addAttribute.clear();
			value = 0.;
			SetDirty();
		}

		ImGui::TreePop();
	}

	bool openSprites = ImGui::TreeNode("sprites");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add flare sprite"))
		{
			object->flareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		if(ImGui::Selectable("Add reverse flare sprite"))
		{
			object->reverseFlareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		if(ImGui::Selectable("Add steering flare sprite"))
		{
			object->steeringFlareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(openSprites)
	{
		RenderSprites("flare sprite", object->flareSprites);
		RenderSprites("reverse flare sprite", object->reverseFlareSprites);
		RenderSprites("steering flare sprite", object->steeringFlareSprites);
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("sounds"))
	{
		RenderSound("flare sound", object->flareSounds);
		RenderSound("reverse flare sound", object->reverseFlareSounds);
		RenderSound("steering flare sound", object->steeringFlareSounds);
		RenderSound("hyperdrive sound", object->hyperSounds);
		RenderSound("hyperdrive in sound", object->hyperInSounds);
		RenderSound("hyperdrive out sound", object->hyperOutSounds);
		RenderSound("jump sound", object->jumpSounds);
		RenderSound("jump in sound", object->jumpInSounds);
		RenderSound("jump out sound", object->jumpOutSounds);
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("effects"))
	{
		RenderEffect("afterburner effect", object->afterburnerEffects);
		RenderEffect("jump effect", object->jumpEffects);
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("weapon"))
	{
		bool isClustered = false;
		if(ImGui::Checkbox("stream", &object->isStreamed))
			SetDirty();
		if(ImGui::Checkbox("cluster", &isClustered))
			SetDirty();
		if(ImGui::Checkbox("safe", &object->isSafe))
			SetDirty();
		if(ImGui::Checkbox("phasing", &object->isPhasing))
			SetDirty();
		if(ImGui::Checkbox("no damage scaling", &object->isDamageScaled))
			SetDirty();
		if(ImGui::Checkbox("parallel", &object->isParallel))
			SetDirty();
		if(ImGui::Checkbox("gravitational", &object->isGravitational))
			SetDirty();
		RenderElement(&object->sprite, "sprite");
		RenderElement(&object->hardpointSprite, "hardpoint sprite");
		static string value;
		if(object->sound)
			value = object->sound->Name();
		if(ImGui::InputText("sound", &value, ImGuiInputTextFlags_EnterReturnsTrue))
			if(Audio::Has(value))
			{
				object->sound = Audio::Get(value);
				SetDirty();
			}
		if(ImGui::TreeNode("ammo"))
		{
			value.clear();
			if(object->ammo.first)
				value = object->ammo.first->Name();
			if(ImGui::InputText("##ammo", &value, ImGuiInputTextFlags_EnterReturnsTrue))
				if(GameData::Outfits().Has(value))
				{
					object->ammo.first = GameData::Outfits().Get(value);
					SetDirty();
				}
			ImGui::SameLine();
			if(ImGui::InputInt("##usage", &object->ammo.second))
				SetDirty();
			ImGui::TreePop();
		}
		value.clear();
		if(object->icon)
			value = object->icon->Name();
		if(ImGui::InputText("icon", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->icon = SpriteSet::Get(value);
			SetDirty();
		}
		RenderEffect("fire effect", object->fireEffects);
		RenderEffect("live effect", object->liveEffects);
		RenderEffect("hit effect", object->hitEffects);
		RenderEffect("target effect", object->targetEffects);
		RenderEffect("die effect", object->dieEffects);

		bool submunitionOpen = ImGui::TreeNode("submunitions");
		if(ImGui::BeginPopupContextItem())
		{
			if(ImGui::Selectable("Add submunition"))
			{
				object->submunitions.emplace_back(nullptr, 1);
				SetDirty();
			}
			ImGui::EndPopup();
		}
		if(submunitionOpen)
		{
			index = 0;
			auto toRemove = object->submunitions.end();
			for(auto it = object->submunitions.begin(); it != object->submunitions.end(); ++it)
			{
				ImGui::PushID(index++);
				static string outfitName;
				outfitName.clear();
				if(it->weapon)
					outfitName = it->weapon->name;
				bool open = ImGui::TreeNode("submunition", "submunition: %s %zu", outfitName.c_str(), it->count);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = it;
					ImGui::EndPopup();
				}
				if(open)
				{
					if(ImGui::InputText("outfit", &outfitName, ImGuiInputTextFlags_EnterReturnsTrue))
						if(GameData::Outfits().Has(outfitName))
						{
							it->weapon = GameData::Outfits().Get(outfitName);
							SetDirty();
						}
					if(ImGui::InputSizeTEx("count", &it->count))
						SetDirty();
					double value = it->facing.Degrees();
					if(ImGui::InputDoubleEx("facing", &value))
					{
						it->facing = Angle(value);
						SetDirty();
					}
					double offset[2] = {it->offset.X(), it->offset.Y()};
					if(ImGui::InputDouble2Ex("offset", offset))
					{
						it->offset.Set(offset[0], offset[1]);
						SetDirty();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if(toRemove != object->submunitions.end())
			{
				object->submunitions.erase(toRemove);
				SetDirty();
			}
			ImGui::TreePop();
		}

		if(ImGui::InputInt("lifetime", &object->lifetime))
		{
			object->lifetime = max(0, object->lifetime);
			SetDirty();
		}
		if(ImGui::InputInt("random lifetime", &object->randomLifetime))
		{
			object->randomLifetime = max(0, object->randomLifetime);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("reload", &object->reload))
		{
			object->reload = max(1., object->reload);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("burst reload", &object->burstReload))
		{
			object->burstReload = max(1., object->burstReload);
			SetDirty();
		}
		if(ImGui::InputInt("burst count", &object->burstCount))
		{
			object->burstCount = max(1, object->burstCount);
			SetDirty();
		}
		if(ImGui::InputInt("homing", &object->homing))
			SetDirty();
		if(ImGui::InputInt("missile strength", &object->missileStrength))
		{
			object->missileStrength = max(0, object->missileStrength);
			SetDirty();
		}
		if(ImGui::InputInt("anti-missile", &object->antiMissile))
		{
			object->antiMissile = max(0, object->antiMissile);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("velocity", &object->velocity))
			SetDirty();
		if(ImGui::InputDoubleEx("random velocity", &object->randomVelocity))
			SetDirty();
		if(ImGui::InputDoubleEx("acceleration", &object->acceleration))
			SetDirty();
		if(ImGui::InputDoubleEx("drag", &object->drag))
			SetDirty();
		double hardpointOffset[2] = {object->hardpointOffset.X(), -object->hardpointOffset.Y()};
		if(ImGui::InputDouble2Ex("hardpoint offset", hardpointOffset))
		{
			object->hardpointOffset.Set(hardpointOffset[0], -hardpointOffset[1]);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("turn", &object->turn))
			SetDirty();
		if(ImGui::InputDoubleEx("inaccuracy", &object->inaccuracy))
			SetDirty();
		if(ImGui::InputDoubleEx("turret turn", &object->turretTurn))
			SetDirty();
		if(ImGui::InputDoubleEx("tracking", &object->tracking))
		{
			object->tracking = max(0., min(1., object->tracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("optical tracking", &object->opticalTracking))
		{
			object->opticalTracking = max(0., min(1., object->opticalTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("infrared tracking", &object->infraredTracking))
		{
			object->infraredTracking = max(0., min(1., object->infraredTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("radar tracking", &object->radarTracking))
		{
			object->radarTracking = max(0., min(1., object->radarTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("firing energy", &object->firingEnergy))
			SetDirty();
		if(ImGui::InputDoubleEx("firing force", &object->firingForce))
			SetDirty();
		if(ImGui::InputDoubleEx("firing fuel", &object->firingFuel))
			SetDirty();
		if(ImGui::InputDoubleEx("firing heat", &object->firingHeat))
			SetDirty();
		if(ImGui::InputDoubleEx("firing hull", &object->firingHull))
			SetDirty();
		if(ImGui::InputDoubleEx("firing shields", &object->firingShields))
			SetDirty();
		if(ImGui::InputDoubleEx("firing ion", &object->firingIon))
			SetDirty();
		if(ImGui::InputDoubleEx("firing slowing", &object->firingSlowing))
			SetDirty();
		if(ImGui::InputDoubleEx("firing disruption", &object->firingDisruption))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing energy", &object->relativeFiringEnergy))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing heat", &object->relativeFiringHeat))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing fuel", &object->relativeFiringFuel))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing hull", &object->relativeFiringHull))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing shields", &object->relativeFiringShields))
			SetDirty();
		if(ImGui::InputDoubleEx("split range", &object->splitRange))
		{
			object->splitRange = max(0., object->splitRange);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("trigger radius", &object->triggerRadius))
		{
			object->triggerRadius = max(0., object->triggerRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("blast radius", &object->blastRadius))
		{
			object->blastRadius = max(0., object->blastRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("shield damage", &object->damage[Weapon::SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hull damage", &object->damage[Weapon::HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("fuel damage", &object->damage[Weapon::FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("heat damage", &object->damage[Weapon::HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("energy damage", &object->damage[Weapon::ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("ion damage", &object->damage[Weapon::ION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("disruption damage", &object->damage[Weapon::DISRUPTION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("slowing damage", &object->damage[Weapon::SLOWING_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relatile shield damage", &object->damage[Weapon::RELATIVE_SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative hull damage", &object->damage[Weapon::RELATIVE_HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative fuel damage", &object->damage[Weapon::RELATIVE_FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative heat damage", &object->damage[Weapon::RELATIVE_HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative energy damage", &object->damage[Weapon::RELATIVE_ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hit force", &object->damage[Weapon::HIT_FORCE]))
			SetDirty();
		if(ImGui::InputDoubleEx("piecing", &object->piercing))
		{
			object->piercing = max(0., object->piercing);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("range override", &object->rangeOverride))
		{
			object->rangeOverride = max(0., object->rangeOverride);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("velocity override", &object->velocityOverride))
		{
			object->velocityOverride = max(0., object->velocityOverride);
			SetDirty();
		}
		double dropoff[2] = {object->damageDropoffRange.first, object->damageDropoffRange.second};
		if(ImGui::InputDouble2Ex("damage dropoff", dropoff))
		{
			object->damageDropoffRange.first = max(0., dropoff[0]);
			object->damageDropoffRange.second = dropoff[1];
			object->hasDamageDropoff = true;
			SetDirty();
		}
		if(ImGui::InputDoubleEx("dropoff modifier", &object->damageDropoffModifier))
		{
			object->damageDropoffModifier = max(0., object->damageDropoffModifier);
			SetDirty();
		}

		if(object->burstReload > object->reload)
			object->burstReload = object->reload;
		if(object->damageDropoffRange.first > object->damageDropoffRange.second)
			object->damageDropoffRange.second = object->Range();
		object->isStreamed |= !(object->MissileStrength() || object->AntiMissile());
		object->isStreamed &= !isClustered;
		ImGui::TreePop();
	}
}



bool OutfitEditor::RenderElement(Body *sprite, const std::string &name)
{
	static string spriteName;
	spriteName.clear();
	bool open = ImGui::TreeNode(name.c_str(), "%s: %s", name.c_str(), sprite->GetSprite() ? sprite->GetSprite()->Name().c_str() : "");
	bool value = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Remove"))
			value = true;
		ImGui::EndPopup();
	}

	if(open)
	{
		if(sprite->GetSprite())
			spriteName = sprite->GetSprite()->Name();
		if(ImGui::InputText("sprite", &spriteName, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			sprite->sprite = SpriteSet::Get(spriteName);
			SetDirty();
		}

		double value = sprite->frameRate * 60.;
		if(ImGui::InputDoubleEx("frame rate", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			sprite->frameRate = value / 60.;
			SetDirty();
		}

		if(ImGui::InputInt("delay", &sprite->delay))
			SetDirty();
		if(ImGui::Checkbox("random start frame", &sprite->randomize))
			SetDirty();
		bool bvalue = !sprite->repeat;
		if(ImGui::Checkbox("no repeat", &bvalue))
			SetDirty();
		sprite->repeat = !bvalue;
		if(ImGui::Checkbox("rewind", &sprite->rewind))
			SetDirty();
		ImGui::TreePop();
	}

	return value;
}



void OutfitEditor::RenderSprites(const string &name, vector<pair<Body, int>> &map)
{
	auto found = map.end();
	int index = 0;
	ImGui::PushID(name.c_str());
	for(auto it = map.begin(); it != map.end(); ++it)
	{
		ImGui::PushID(index++);
		if(RenderElement(&it->first, name))
			found = it;
		ImGui::PopID();
	}
	ImGui::PopID();

	if(found != map.end())
	{
		map.erase(found);
		SetDirty();
	}
}



void OutfitEditor::RenderSound(const string &name, map<const Sound *, int> &map)
{
	static string soundName;
	soundName.clear();

	bool open = ImGui::TreeNode(name.c_str());
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Sound"))
		{
			map.emplace(nullptr, 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(open)
	{
		const Sound *toAdd = nullptr;
		const Sound *toRemove = nullptr;
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			soundName = it->first ? it->first->Name() : "";
			if(ImGui::InputText("##sound", &soundName, ImGuiInputTextFlags_EnterReturnsTrue))
				if(Audio::Has(soundName))
				{
					toAdd = Audio::Get(soundName);
					toRemove = it->first;
					SetDirty();
				}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it->first;
				SetDirty();
			}
			ImGui::PopID();
		}

		if(toAdd)
		{
			int oldCount = map[toRemove];
			map[toAdd] += oldCount;
		}
		if(toRemove)
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



void OutfitEditor::RenderEffect(const string &name, map<const Effect *, int> &map)
{
	static string effectName;
	effectName.clear();

	bool open = ImGui::TreeNode(name.c_str());
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Effect"))
		{
			map.emplace(nullptr, 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(open)
	{
		const Effect *toAdd = nullptr;
		const Effect *toRemove = nullptr;
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			effectName = it->first ? it->first->Name() : "";
			if(ImGui::InputText("##effect", &effectName, ImGuiInputTextFlags_EnterReturnsTrue))
				if(GameData::Effects().Has(effectName))
				{
					toAdd = GameData::Effects().Get(effectName);
					toRemove = it->first;
					SetDirty();
				}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it->first;
				SetDirty();
			}
			ImGui::PopID();
		}

		if(toAdd)
		{
			int oldCount = map[toRemove];
			map[toAdd] += oldCount;
		}
		if(toRemove)
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



void OutfitEditor::WriteToFile(DataWriter &writer, const Outfit *outfit)
{
	writer.Write("outfit", outfit->Name());
	writer.BeginChild();
	writer.Write("category", object->Category().c_str());
	if(object->pluralName != outfit->name + "s")
		writer.Write("plural", object->pluralName);
	writer.Write("cost", object->cost);
	writer.Write("mass", object->mass);
	if(object->flotsamSprite)
		writer.Write("floatsam sprite", object->flotsamSprite->Name());
	if(object->thumbnail)
		writer.Write("thumbnail", object->thumbnail->Name());
	if(!object->licenses.empty())
	{
		writer.WriteToken("licenses");
		for(auto &&license : object->licenses)
			writer.WriteToken(license);
		writer.Write();
	}
	for(auto &&attribute : object->attributes)
		writer.Write(attribute.first, attribute.second);
	for(auto &&flareSprite : object->flareSprites)
		for(int i = 0; i < flareSprite.second; ++i)
			flareSprite.first.SaveSprite(writer, "flare sprite");
	for(auto &&reverseFlareSprite : object->reverseFlareSprites)
		for(int i = 0; i < reverseFlareSprite.second; ++i)
			reverseFlareSprite.first.SaveSprite(writer, "reverse flare sprite");
	for(auto &&steeringFlareSprite : object->steeringFlareSprites)
		for(int i = 0; i < steeringFlareSprite.second; ++i)
			steeringFlareSprite.first.SaveSprite(writer, "steering flare sprite");
	for(auto &&flareSound : object->flareSounds)
		for(int i = 0; i < flareSound.second; ++i)
			writer.Write("flare sound", flareSound.first->Name());
	for(auto &&reverseFlareSound : object->reverseFlareSounds)
		for(int i = 0; i < reverseFlareSound.second; ++i)
			writer.Write("reverse flare sound", reverseFlareSound.first->Name());
	for(auto &&steeringFlareSound : object->steeringFlareSounds)
		for(int i = 0; i < steeringFlareSound.second; ++i)
			writer.Write("steering flare sound", steeringFlareSound.first->Name());
	for(auto &&hyperdriveSound : object->hyperSounds)
		for(int i = 0; i < hyperdriveSound.second; ++i)
			writer.Write("hyperdrive sound", hyperdriveSound.first->Name());
	for(auto &&hyperdriveInSound : object->hyperInSounds)
		for(int i = 0; i < hyperdriveInSound.second; ++i)
			writer.Write("hyperdrive in sound", hyperdriveInSound.first->Name());
	for(auto &&hyperdriveOutSound : object->hyperInSounds)
		for(int i = 0; i < hyperdriveOutSound.second; ++i)
			writer.Write("hyperdrive out sound", hyperdriveOutSound.first->Name());
	for(auto &&jumpSound : object->jumpSounds)
		for(int i = 0; i < jumpSound.second; ++i)
			writer.Write("jump sound", jumpSound.first->Name());
	for(auto &&jumpInSound : object->jumpInSounds)
		for(int i = 0; i < jumpInSound.second; ++i)
			writer.Write("jump in sound", jumpInSound.first->Name());
	for(auto &&jumpOutSound : object->jumpOutSounds)
		for(int i = 0; i < jumpOutSound.second; ++i)
			writer.Write("jump out sound", jumpOutSound.first->Name());
	for(auto &&afterburnerEffect : object->afterburnerEffects)
		for(int i = 0; i < afterburnerEffect.second; ++i)
			writer.Write("afterburner effect", afterburnerEffect.first->Name());
	for(auto &&jumpEffect : object->jumpEffects)
		for(int i = 0; i < jumpEffect.second; ++i)
			writer.Write("jump effect", jumpEffect.first->Name());

	if(object->IsWeapon())
	{
		writer.Write("weapon");
		writer.BeginChild();
		if((object->MissileStrength() || object->AntiMissile()) && object->isStreamed)
			writer.Write("stream");
		if(!object->MissileStrength() && !object->AntiMissile() && !object->isStreamed)
			writer.Write("clustered");
		if(object->isSafe)
			writer.Write("safe");
		if(object->isPhasing)
			writer.Write("phasing");
		if(!object->isDamageScaled)
			writer.Write("no damage scaling");
		if(object->isParallel)
			writer.Write("parallel");
		if(object->isGravitational)
			writer.Write("gravitational");
		if(object->sprite.HasSprite())
			object->sprite.SaveSprite(writer);
		if(object->hardpointSprite.HasSprite())
			object->hardpointSprite.SaveSprite(writer, "hardpoint sprite");
		if(object->sound)
			writer.Write("sound", object->sound->Name());
		if(object->ammo.first)
		{
			writer.WriteToken("ammo");
			writer.WriteToken(object->ammo.first->Name());
			if(object->ammo.second != 1)
				writer.WriteToken(object->ammo.second);
			writer.Write();
		}
		if(object->icon)
			writer.Write("icon", object->icon->Name());

		for(auto &&fireEffect : object->fireEffects)
			writer.Write("fire effect", fireEffect.first->Name(), fireEffect.second);
		for(auto &&liveEffect : object->liveEffects)
			writer.Write("live effect", liveEffect.first->Name(), liveEffect.second);
		for(auto &&hitEffect : object->hitEffects)
			writer.Write("hit effect", hitEffect.first->Name(), hitEffect.second);
		for(auto &&targetEffect : object->targetEffects)
			writer.Write("target effect", targetEffect.first->Name(), targetEffect.second);
		for(auto &&dieEffect : object->dieEffects)
			writer.Write("die effect", dieEffect.first->Name(), dieEffect.second);

		for(auto &&submunition : object->submunitions)
		{
			writer.WriteToken("submunition");
			writer.WriteToken(submunition.weapon->Name());
			if(submunition.count > 1)
				writer.WriteToken(submunition.count);
			writer.Write();
			writer.BeginChild();
			if(submunition.facing.Degrees())
				writer.Write("facing", submunition.facing.Degrees());
			if(submunition.offset)
				writer.Write("offset", submunition.offset.X(), submunition.offset.Y());
			writer.EndChild();
		}

		if(object->lifetime)
			writer.Write("lifetime", object->lifetime);
		if(object->randomLifetime)
			writer.Write("random lifetime", object->randomLifetime);
		if(object->reload)
			writer.Write("reload", object->reload);
		if(object->burstReload > 1.)
			writer.Write("burst reload", object->burstReload);
		if(object->burstCount > 1)
			writer.Write("burst count", object->burstCount);
		if(object->homing)
			writer.Write("homing", object->homing);
		if(object->missileStrength)
			writer.Write("missile strength", object->missileStrength);
		if(object->antiMissile)
			writer.Write("anti-missile", object->antiMissile);
		if(object->velocity)
			writer.Write("velocity", object->velocity);
		if(object->randomVelocity)
			writer.Write("random velocity", object->randomVelocity);
		if(object->acceleration)
			writer.Write("acceleration", object->acceleration);
		if(object->drag)
			writer.Write("drag", object->drag);
		if(object->hardpointOffset)
			writer.Write("hardpoint offset", object->hardpointOffset.X(), object->hardpointOffset.Y());
		if(object->turn)
			writer.Write("turn", object->turn);
		if(object->inaccuracy)
			writer.Write("inaccuracy", object->inaccuracy);
		if(object->turretTurn)
			writer.Write("turret turn", object->turretTurn);
		if(object->tracking)
			writer.Write("tracking", object->tracking);
		if(object->opticalTracking)
			writer.Write("optical tracking", object->opticalTracking);
		if(object->infraredTracking)
			writer.Write("infrared tracking", object->infraredTracking);
		if(object->radarTracking)
			writer.Write("radar tracking", object->radarTracking);
		if(object->firingEnergy)
			writer.Write("firing energy", object->firingEnergy);
		if(object->firingForce)
			writer.Write("firing force", object->firingForce);
		if(object->firingFuel)
			writer.Write("firing fuel", object->firingFuel);
		if(object->firingHeat)
			writer.Write("firing heat", object->firingHeat);
		if(object->firingHull)
			writer.Write("firing hull", object->firingHull);
		if(object->firingShields)
			writer.Write("firing shields", object->firingShields);
		if(object->firingIon)
			writer.Write("firing ion", object->firingIon);
		if(object->firingSlowing)
			writer.Write("firing slowing", object->firingSlowing);
		if(object->firingDisruption)
			writer.Write("firing disruption", object->firingDisruption);
		if(object->relativeFiringEnergy)
			writer.Write("relative firing energy", object->relativeFiringEnergy);
		if(object->relativeFiringHeat)
			writer.Write("relative firing heat", object->relativeFiringHeat);
		if(object->relativeFiringFuel)
			writer.Write("relative firing fuel", object->relativeFiringFuel);
		if(object->relativeFiringHull)
			writer.Write("relative firing hull", object->relativeFiringHull);
		if(object->relativeFiringShields)
			writer.Write("relative firing shields", object->relativeFiringShields);
		if(object->splitRange)
			writer.Write("split range", object->splitRange);
		if(object->triggerRadius)
			writer.Write("trigger radius", object->triggerRadius);
		if(object->blastRadius)
			writer.Write("blast radius", object->blastRadius);
		if(object->damage[Weapon::SHIELD_DAMAGE])
			writer.Write("shield damage", object->damage[Weapon::SHIELD_DAMAGE]);
		if(object->damage[Weapon::HULL_DAMAGE])
			writer.Write("hull damage", object->damage[Weapon::HULL_DAMAGE]);
		if(object->damage[Weapon::FUEL_DAMAGE])
			writer.Write("fuel damage", object->damage[Weapon::FUEL_DAMAGE]);
		if(object->damage[Weapon::HEAT_DAMAGE])
			writer.Write("heat damage", object->damage[Weapon::HEAT_DAMAGE]);
		if(object->damage[Weapon::ENERGY_DAMAGE])
			writer.Write("energy damage", object->damage[Weapon::ENERGY_DAMAGE]);
		if(object->damage[Weapon::ION_DAMAGE])
			writer.Write("ion damage", object->damage[Weapon::ION_DAMAGE]);
		if(object->damage[Weapon::DISRUPTION_DAMAGE])
			writer.Write("disruption damage", object->damage[Weapon::DISRUPTION_DAMAGE]);
		if(object->damage[Weapon::SLOWING_DAMAGE])
			writer.Write("slowing damage", object->damage[Weapon::SLOWING_DAMAGE]);
		if(object->damage[Weapon::RELATIVE_SHIELD_DAMAGE])
			writer.Write("relative shield damage", object->damage[Weapon::RELATIVE_SHIELD_DAMAGE]);
		if(object->damage[Weapon::RELATIVE_HULL_DAMAGE])
			writer.Write("relative hull damage", object->damage[Weapon::RELATIVE_HULL_DAMAGE]);
		if(object->damage[Weapon::RELATIVE_FUEL_DAMAGE])
			writer.Write("relative fuel damage", object->damage[Weapon::RELATIVE_FUEL_DAMAGE]);
		if(object->damage[Weapon::RELATIVE_HEAT_DAMAGE])
			writer.Write("relative heat damage", object->damage[Weapon::RELATIVE_HEAT_DAMAGE]);
		if(object->damage[Weapon::RELATIVE_ENERGY_DAMAGE])
			writer.Write("relative energy damage", object->damage[Weapon::RELATIVE_ENERGY_DAMAGE]);
		if(object->damage[Weapon::HIT_FORCE])
			writer.Write("hit force", object->damage[Weapon::HIT_FORCE]);
		if(object->piercing)
			writer.Write("piercing", object->piercing);
		if(object->rangeOverride)
			writer.Write("range override", object->rangeOverride);
		if(object->velocityOverride)
			writer.Write("velocity override", object->velocityOverride);
		if(object->hasDamageDropoff)
			writer.Write("damage dropoff", object->damageDropoffRange.first, object->damageDropoffRange.second);
		if(object->damageDropoffModifier)
			writer.Write("dropoff modifier", object->damageDropoffModifier);
		writer.EndChild();
	}
	if(!object->description.empty())
	{
		size_t newline = object->description.find('\n');
		size_t start = 0;
		do {
			writer.Write("description", object->description.substr(start, newline - start));
			start = newline + 1;
			newline = object->description.find('\n', start);
		} while(newline != string::npos);
	}
	writer.EndChild();
}
