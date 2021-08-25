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
	if(!ImGui::Begin("Outfit Editor", &show, ImGuiWindowFlags_MenuBar))
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
			if(ImGui::MenuItem("Add to Cargo", nullptr, false, object && editor.Player().Cargo().Free() >= -object->Attributes().Get("outfit space")))
				editor.Player().Cargo().Add(object);
			if(!object || editor.Player().Cargo().Free() < -object->Attributes().Get("outfit space"))
				if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if(!object)
						ImGui::SetTooltip("Select an outfit first.");
					else
						ImGui::SetTooltip("Not enough space in your cargo hold for this outfit.");
				}
			if(ImGui::MenuItem("Add to Flagship", nullptr, false, object && editor.Player().Flagship()->Attributes().CanAdd(*object, 1) > 0))
				editor.Player().Flagship()->AddOutfit(object, 1);
			if(!object || editor.Player().Flagship()->Attributes().CanAdd(*object, 1) <= 0)
				if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if(!object)
						ImGui::SetTooltip("Select an outfit first.");
					else
						ImGui::SetTooltip("Not enough outfit space in your flagship.");
				}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(ImGui::InputCombo("outfit", &searchBox, &object, GameData::Outfits()))
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
	static Sprite *floatsamSprite = nullptr;
	if(ImGui::InputCombo("floatsam sprite", &str, &floatsamSprite, SpriteSet::GetSprites()))
	{
		object->flotsamSprite = floatsamSprite;
		SetDirty();
	}

	str.clear();
	if(object->thumbnail)
		str = object->thumbnail->Name();
	static Sprite *thumbnailSprite = nullptr;
	if(ImGui::InputCombo("thumbnail", &str, &thumbnailSprite, SpriteSet::GetSprites()))
	{
		object->thumbnail = thumbnailSprite;
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
		static Sprite *iconSprite = nullptr;
		if(ImGui::InputCombo("icon", &value, &iconSprite, SpriteSet::GetSprites()))
		{
			object->icon = iconSprite;
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



void OutfitEditor::WriteToFile(DataWriter &writer, const Outfit *outfit)
{
	writer.Write("outfit", outfit->Name());
	writer.BeginChild();
	writer.Write("category", outfit->Category().c_str());
	if(outfit->pluralName != outfit->name + "s")
		writer.Write("plural", outfit->pluralName);
	writer.Write("cost", outfit->cost);
	writer.Write("mass", outfit->mass);
	if(outfit->flotsamSprite)
		writer.Write("floatsam sprite", outfit->flotsamSprite->Name());
	if(outfit->thumbnail)
		writer.Write("thumbnail", outfit->thumbnail->Name());
	if(!outfit->licenses.empty())
	{
		writer.WriteToken("licenses");
		for(auto &&license : outfit->licenses)
			writer.WriteToken(license);
		writer.Write();
	}
	for(auto &&attribute : outfit->attributes)
		writer.Write(attribute.first, attribute.second);
	for(auto &&flareSprite : outfit->flareSprites)
		for(int i = 0; i < flareSprite.second; ++i)
			flareSprite.first.SaveSprite(writer, "flare sprite");
	for(auto &&reverseFlareSprite : outfit->reverseFlareSprites)
		for(int i = 0; i < reverseFlareSprite.second; ++i)
			reverseFlareSprite.first.SaveSprite(writer, "reverse flare sprite");
	for(auto &&steeringFlareSprite : outfit->steeringFlareSprites)
		for(int i = 0; i < steeringFlareSprite.second; ++i)
			steeringFlareSprite.first.SaveSprite(writer, "steering flare sprite");
	for(auto &&flareSound : outfit->flareSounds)
		for(int i = 0; i < flareSound.second; ++i)
			writer.Write("flare sound", flareSound.first->Name());
	for(auto &&reverseFlareSound : outfit->reverseFlareSounds)
		for(int i = 0; i < reverseFlareSound.second; ++i)
			writer.Write("reverse flare sound", reverseFlareSound.first->Name());
	for(auto &&steeringFlareSound : outfit->steeringFlareSounds)
		for(int i = 0; i < steeringFlareSound.second; ++i)
			writer.Write("steering flare sound", steeringFlareSound.first->Name());
	for(auto &&hyperdriveSound : outfit->hyperSounds)
		for(int i = 0; i < hyperdriveSound.second; ++i)
			writer.Write("hyperdrive sound", hyperdriveSound.first->Name());
	for(auto &&hyperdriveInSound : outfit->hyperInSounds)
		for(int i = 0; i < hyperdriveInSound.second; ++i)
			writer.Write("hyperdrive in sound", hyperdriveInSound.first->Name());
	for(auto &&hyperdriveOutSound : outfit->hyperInSounds)
		for(int i = 0; i < hyperdriveOutSound.second; ++i)
			writer.Write("hyperdrive out sound", hyperdriveOutSound.first->Name());
	for(auto &&jumpSound : outfit->jumpSounds)
		for(int i = 0; i < jumpSound.second; ++i)
			writer.Write("jump sound", jumpSound.first->Name());
	for(auto &&jumpInSound : outfit->jumpInSounds)
		for(int i = 0; i < jumpInSound.second; ++i)
			writer.Write("jump in sound", jumpInSound.first->Name());
	for(auto &&jumpOutSound : outfit->jumpOutSounds)
		for(int i = 0; i < jumpOutSound.second; ++i)
			writer.Write("jump out sound", jumpOutSound.first->Name());
	for(auto &&afterburnerEffect : outfit->afterburnerEffects)
		for(int i = 0; i < afterburnerEffect.second; ++i)
			writer.Write("afterburner effect", afterburnerEffect.first->Name());
	for(auto &&jumpEffect : outfit->jumpEffects)
		for(int i = 0; i < jumpEffect.second; ++i)
			writer.Write("jump effect", jumpEffect.first->Name());

	if(outfit->IsWeapon())
	{
		writer.Write("weapon");
		writer.BeginChild();
		if((outfit->MissileStrength() || outfit->AntiMissile()) && outfit->isStreamed)
			writer.Write("stream");
		if(!outfit->MissileStrength() && !outfit->AntiMissile() && !outfit->isStreamed)
			writer.Write("clustered");
		if(outfit->isSafe)
			writer.Write("safe");
		if(outfit->isPhasing)
			writer.Write("phasing");
		if(!outfit->isDamageScaled)
			writer.Write("no damage scaling");
		if(outfit->isParallel)
			writer.Write("parallel");
		if(outfit->isGravitational)
			writer.Write("gravitational");
		if(outfit->sprite.HasSprite())
			outfit->sprite.SaveSprite(writer);
		if(outfit->hardpointSprite.HasSprite())
			outfit->hardpointSprite.SaveSprite(writer, "hardpoint sprite");
		if(outfit->sound)
			writer.Write("sound", outfit->sound->Name());
		if(outfit->ammo.first)
		{
			writer.WriteToken("ammo");
			writer.WriteToken(outfit->ammo.first->Name());
			if(outfit->ammo.second != 1)
				writer.WriteToken(outfit->ammo.second);
			writer.Write();
		}
		if(outfit->icon)
			writer.Write("icon", outfit->icon->Name());

		for(auto &&fireEffect : outfit->fireEffects)
			writer.Write("fire effect", fireEffect.first->Name(), fireEffect.second);
		for(auto &&liveEffect : outfit->liveEffects)
			writer.Write("live effect", liveEffect.first->Name(), liveEffect.second);
		for(auto &&hitEffect : outfit->hitEffects)
			writer.Write("hit effect", hitEffect.first->Name(), hitEffect.second);
		for(auto &&targetEffect : outfit->targetEffects)
			writer.Write("target effect", targetEffect.first->Name(), targetEffect.second);
		for(auto &&dieEffect : outfit->dieEffects)
			writer.Write("die effect", dieEffect.first->Name(), dieEffect.second);

		for(auto &&submunition : outfit->submunitions)
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

		if(outfit->lifetime)
			writer.Write("lifetime", outfit->lifetime);
		if(outfit->randomLifetime)
			writer.Write("random lifetime", outfit->randomLifetime);
		if(outfit->reload)
			writer.Write("reload", outfit->reload);
		if(outfit->burstReload > 1.)
			writer.Write("burst reload", outfit->burstReload);
		if(outfit->burstCount > 1)
			writer.Write("burst count", outfit->burstCount);
		if(outfit->homing)
			writer.Write("homing", outfit->homing);
		if(outfit->missileStrength)
			writer.Write("missile strength", outfit->missileStrength);
		if(outfit->antiMissile)
			writer.Write("anti-missile", outfit->antiMissile);
		if(outfit->velocity)
			writer.Write("velocity", outfit->velocity);
		if(outfit->randomVelocity)
			writer.Write("random velocity", outfit->randomVelocity);
		if(outfit->acceleration)
			writer.Write("acceleration", outfit->acceleration);
		if(outfit->drag)
			writer.Write("drag", outfit->drag);
		if(outfit->hardpointOffset)
			writer.Write("hardpoint offset", outfit->hardpointOffset.X(), outfit->hardpointOffset.Y());
		if(outfit->turn)
			writer.Write("turn", outfit->turn);
		if(outfit->inaccuracy)
			writer.Write("inaccuracy", outfit->inaccuracy);
		if(outfit->turretTurn)
			writer.Write("turret turn", outfit->turretTurn);
		if(outfit->tracking)
			writer.Write("tracking", outfit->tracking);
		if(outfit->opticalTracking)
			writer.Write("optical tracking", outfit->opticalTracking);
		if(outfit->infraredTracking)
			writer.Write("infrared tracking", outfit->infraredTracking);
		if(outfit->radarTracking)
			writer.Write("radar tracking", outfit->radarTracking);
		if(outfit->firingEnergy)
			writer.Write("firing energy", outfit->firingEnergy);
		if(outfit->firingForce)
			writer.Write("firing force", outfit->firingForce);
		if(outfit->firingFuel)
			writer.Write("firing fuel", outfit->firingFuel);
		if(outfit->firingHeat)
			writer.Write("firing heat", outfit->firingHeat);
		if(outfit->firingHull)
			writer.Write("firing hull", outfit->firingHull);
		if(outfit->firingShields)
			writer.Write("firing shields", outfit->firingShields);
		if(outfit->firingIon)
			writer.Write("firing ion", outfit->firingIon);
		if(outfit->firingSlowing)
			writer.Write("firing slowing", outfit->firingSlowing);
		if(outfit->firingDisruption)
			writer.Write("firing disruption", outfit->firingDisruption);
		if(outfit->relativeFiringEnergy)
			writer.Write("relative firing energy", outfit->relativeFiringEnergy);
		if(outfit->relativeFiringHeat)
			writer.Write("relative firing heat", outfit->relativeFiringHeat);
		if(outfit->relativeFiringFuel)
			writer.Write("relative firing fuel", outfit->relativeFiringFuel);
		if(outfit->relativeFiringHull)
			writer.Write("relative firing hull", outfit->relativeFiringHull);
		if(outfit->relativeFiringShields)
			writer.Write("relative firing shields", outfit->relativeFiringShields);
		if(outfit->splitRange)
			writer.Write("split range", outfit->splitRange);
		if(outfit->triggerRadius)
			writer.Write("trigger radius", outfit->triggerRadius);
		if(outfit->blastRadius)
			writer.Write("blast radius", outfit->blastRadius);
		if(outfit->damage[Weapon::SHIELD_DAMAGE])
			writer.Write("shield damage", outfit->damage[Weapon::SHIELD_DAMAGE]);
		if(outfit->damage[Weapon::HULL_DAMAGE])
			writer.Write("hull damage", outfit->damage[Weapon::HULL_DAMAGE]);
		if(outfit->damage[Weapon::FUEL_DAMAGE])
			writer.Write("fuel damage", outfit->damage[Weapon::FUEL_DAMAGE]);
		if(outfit->damage[Weapon::HEAT_DAMAGE])
			writer.Write("heat damage", outfit->damage[Weapon::HEAT_DAMAGE]);
		if(outfit->damage[Weapon::ENERGY_DAMAGE])
			writer.Write("energy damage", outfit->damage[Weapon::ENERGY_DAMAGE]);
		if(outfit->damage[Weapon::ION_DAMAGE])
			writer.Write("ion damage", outfit->damage[Weapon::ION_DAMAGE]);
		if(outfit->damage[Weapon::DISRUPTION_DAMAGE])
			writer.Write("disruption damage", outfit->damage[Weapon::DISRUPTION_DAMAGE]);
		if(outfit->damage[Weapon::SLOWING_DAMAGE])
			writer.Write("slowing damage", outfit->damage[Weapon::SLOWING_DAMAGE]);
		if(outfit->damage[Weapon::RELATIVE_SHIELD_DAMAGE])
			writer.Write("relative shield damage", outfit->damage[Weapon::RELATIVE_SHIELD_DAMAGE]);
		if(outfit->damage[Weapon::RELATIVE_HULL_DAMAGE])
			writer.Write("relative hull damage", outfit->damage[Weapon::RELATIVE_HULL_DAMAGE]);
		if(outfit->damage[Weapon::RELATIVE_FUEL_DAMAGE])
			writer.Write("relative fuel damage", outfit->damage[Weapon::RELATIVE_FUEL_DAMAGE]);
		if(outfit->damage[Weapon::RELATIVE_HEAT_DAMAGE])
			writer.Write("relative heat damage", outfit->damage[Weapon::RELATIVE_HEAT_DAMAGE]);
		if(outfit->damage[Weapon::RELATIVE_ENERGY_DAMAGE])
			writer.Write("relative energy damage", outfit->damage[Weapon::RELATIVE_ENERGY_DAMAGE]);
		if(outfit->damage[Weapon::HIT_FORCE])
			writer.Write("hit force", outfit->damage[Weapon::HIT_FORCE]);
		if(outfit->piercing)
			writer.Write("piercing", outfit->piercing);
		if(outfit->rangeOverride)
			writer.Write("range override", outfit->rangeOverride);
		if(outfit->velocityOverride)
			writer.Write("velocity override", outfit->velocityOverride);
		if(outfit->hasDamageDropoff)
			writer.Write("damage dropoff", outfit->damageDropoffRange.first, outfit->damageDropoffRange.second);
		if(outfit->damageDropoffModifier)
			writer.Write("dropoff modifier", outfit->damageDropoffModifier);
		writer.EndChild();
	}
	if(!outfit->description.empty())
	{
		size_t newline = outfit->description.find('\n');
		size_t start = 0;
		do {
			writer.Write("description", outfit->description.substr(start, newline - start));
			start = newline + 1;
			newline = outfit->description.find('\n', start);
		} while(newline != string::npos);
	}
	writer.EndChild();
}
