/* SystemEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SystemEditor.h"

#include "DataFile.h"
#include "DataWriter.h"
#include "Editor.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MainEditorPanel.h"
#include "MapPanel.h"
#include "MapEditorPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"

using namespace std;



SystemEditor::SystemEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<System>(editor, show)
{
}



void SystemEditor::UpdateSystemPosition(const System *system, Point dp)
{
	const_cast<System *>(system)->position += dp;
	SetDirty(system);
}



void SystemEditor::UpdateStellarPosition(const StellarObject &object, Point dp)
{
	auto &obj = const_cast<StellarObject &>(object);
	double now = editor.Player().GetDate().DaysSinceEpoch();

	auto newPos = obj.position + dp;
	if(obj.parent != -1)
		newPos -= this->object->objects[obj.parent].position;

	obj.distance = newPos.Length();
	Angle newAngle(newPos);

	obj.speed = (newAngle.Degrees() - obj.offset) / now;
	this->object->SetDate(editor.Player().GetDate());

	SetDirty(this->object);
}



void SystemEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("System Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Open Editing Panel"))
			{
				shared_ptr<MapEditorPanel> panel(new MapEditorPanel(editor.Player(), this));
				editor.GetMenu().Push(panel);
				mapEditor = panel;
			}
			if(ImGui::MenuItem("Open In-System Editor"))
			{
				shared_ptr<MainEditorPanel> panel(new MainEditorPanel(editor.Player(), this));
				editor.GetMenu().Push(panel);
				stellarEditor = panel;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(!mapEditor.expired())
		object = const_cast<System *>(mapEditor.lock()->Selected());
	if(!stellarEditor.expired())
		object = const_cast<System *>(stellarEditor.lock()->Selected());

	if(ImGui::InputText("system", &searchBox))
		if(auto *ptr = GameData::Systems().Find(searchBox))
		{
			object = const_cast<System *>(ptr);
			searchBox.clear();
			if(auto map = mapEditor.lock())
				map->Select(object);
			if(auto map = stellarEditor.lock())
				map->Select(object);
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
				ImGui::SetTooltip("Select a system first.");
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
				ImGui::SetTooltip("Input the new name for the system above.");
			else if(!object)
				ImGui::SetTooltip("Select a system first.");
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
				ImGui::SetTooltip("Select a system first.");
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
		*object = *GameData::defaultSystems.Get(object->name);
		for(auto &&link : object->links)
			const_cast<System *>(link)->Link(object);
		UpdateMap();
		SetClean();
	} 
	if(clone)
	{
		auto *clone = const_cast<System *>(GameData::Systems().Get(searchBox));
		*clone = *object;
		object = clone;

		object->name = searchBox;
		object->position += Point(100., 0.);
		object->objects.clear();
		object->links.clear();
		object->attributes.insert("uninhabited");
		GameData::UpdateSystems();
		for(auto &&link : editor.Player().visitedSystems)
			editor.Player().Visit(*link);
		UpdateMap(/*updateSystem=*/false);
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderSystem();
	ImGui::End();
}



void SystemEditor::RenderSystem()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());
	if(ImGui::Checkbox("hidden", &object->hidden))
		SetDirty();

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "uninhabited")
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
			object->attributes.insert(attribute);
		for(auto &&attribute : toRemove)
			object->attributes.erase(attribute);
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##system", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->attributes.insert(move(addAttribute));
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("links"))
	{
		set<System *> toAdd;
		set<System *> toRemove;
		index = 0;
		for(auto &link : object->links)
		{
			ImGui::PushID(index++);
			string name = link->Name();
			if(ImGui::InputText("link", &name, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				auto *system = const_cast<System *>(GameData::Systems().Find(name));
				if(system)
					toAdd.insert(system);
				toRemove.insert(const_cast<System *>(link));
			}
			ImGui::PopID();
		}
		string addLink;
		if(ImGui::InputText("add link", &addLink, ImGuiInputTextFlags_EnterReturnsTrue))
			if(auto *system = const_cast<System *>(GameData::Systems().Find(addLink)))
				toAdd.insert(system);

		for(auto &sys : toAdd)
		{
			object->Link(sys);
			SetDirty(sys);
		}
		for(auto &&sys : toRemove)
		{
			object->Unlink(sys);
			SetDirty(sys);
		}
		if(!toAdd.empty() || !toRemove.empty())
		{
			SetDirty();
			UpdateMap();
		}
		ImGui::TreePop();
	}

	bool asteroidsOpen = ImGui::TreeNode("asteroids");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Asteroid"))
		{
			object->asteroids.emplace_back("small rock", 1, 1.);
			if(auto stellars = stellarEditor.lock())
				stellars->UpdateCache();
			SetDirty();
		}
		if(ImGui::Selectable("Add Mineable"))
		{
			object->asteroids.emplace_back(&GameData::Minables().begin()->second, 1, 1.);
			if(auto stellars = stellarEditor.lock())
				stellars->UpdateCache();
			SetDirty();
		}
		ImGui::EndPopup();
	}

	if(asteroidsOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &asteroid : object->asteroids)
		{
			ImGui::PushID(index);
			if(asteroid.Type())
			{
				bool open = ImGui::TreeNode("minables");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = index;
					ImGui::EndPopup();
				}

				if(open)
				{
					ImGui::SetNextItemWidth(300.f);
					if(ImGui::BeginCombo("##minables", asteroid.Type()->Name().c_str()))
					{
						int index = 0;
						for(const auto &item : GameData::Minables())
						{
							const bool selected = &item.second == asteroid.Type();
							if(ImGui::Selectable(item.first.c_str(), selected))
							{
								asteroid.type = &item.second;
								if(auto stellars = stellarEditor.lock())
									stellars->UpdateCache();
								SetDirty();
							}
							++index;

							if(selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputInt("##count", &asteroid.count))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						SetDirty();
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputDoubleEx("##energy", &asteroid.energy))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				bool open = ImGui::TreeNode("asteroids");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						toRemove = index;
					}
					ImGui::EndPopup();
				}

				if(open)
				{
					ImGui::SetNextItemWidth(300.f);
					if(ImGui::InputText("##asteroids", &asteroid.name))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						SetDirty();
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputInt("##count", &asteroid.count))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						SetDirty();
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputDoubleEx("##energy", &asteroid.energy))
					{
						if(auto stellars = stellarEditor.lock())
							stellars->UpdateCache();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->asteroids.erase(object->asteroids.begin() + toRemove);
			if(auto stellars = stellarEditor.lock())
				stellars->UpdateCache();
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool fleetOpen = ImGui::TreeNode("fleets");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Fleet"))
			object->fleets.emplace_back(&GameData::Fleets().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(fleetOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &fleet : object->fleets)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("fleet");
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				if(ImGui::BeginCombo("##fleets", fleet.Get()->Name().c_str()))
				{
					int index = 0;
					for(const auto &item : GameData::Fleets())
					{
						const bool selected = &item.second == fleet.Get();
						if(ImGui::Selectable(item.first.c_str(), selected))
						{
							fleet.fleet = &item.second;
							SetDirty();
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if(ImGui::InputInt("##period", &fleet.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			object->fleets.erase(object->fleets.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool hazardOpen = ImGui::TreeNode("hazards");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Hazard"))
			object->hazards.emplace_back(&GameData::Hazards().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(hazardOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &hazard : object->hazards)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("hazard");
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				if(ImGui::BeginCombo("##hazards", hazard.Get()->Name().c_str()))
				{
					int index = 0;
					for(const auto &item : GameData::Hazards())
					{
						const bool selected = &item.second == hazard.Get();
						if(ImGui::Selectable(item.first.c_str(), selected))
						{
							hazard.hazard = &item.second;
							SetDirty();
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if(ImGui::InputInt("##period", &hazard.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->hazards.erase(object->hazards.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	double pos[2] = {object->position.X(), object->Position().Y()};
	bool updatedPos = false;
	if(ImGui::InputDouble2Ex("pos", pos, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->position.Set(pos[0], pos[1]);
		updatedPos = true;
	}
	ImGui::SameLine();
	auto *mapPanel = dynamic_cast<MapPanel *>(editor.GetUI().Top().get());
	if(!mapPanel)
		ImGui::PushDisabled();
	if(ImGui::Button("set to last click pos"))
	{
		object->position = mapPanel->click;
		updatedPos = true;
	}
	if(!mapPanel)
		ImGui::PopDisabled();
	if(updatedPos)
	{
		mapPanel->UpdateCache();
		SetDirty();
	}

	{
		if(ImGui::BeginCombo("governments", object->government ? object->government->TrueName().c_str() : ""))
		{
			for(const auto &government : GameData::Governments())
			{
				const bool selected = &government.second == object->government;
				if(ImGui::Selectable(government.first.c_str(), selected))
				{
					object->government = &government.second;
					UpdateMap(/*updateSystems=*/false);
					SetDirty();
				}

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if(ImGui::InputText("music", &object->music))
		SetDirty();

	if(ImGui::InputDoubleEx("habitable", &object->habitable))
		SetDirty();
	if(ImGui::InputDoubleEx("belt", &object->asteroidBelt))
		SetDirty();
	if(ImGui::InputDoubleEx("jump range", &object->jumpRange))
		SetDirty();
	if(object->jumpRange < 0.)
		object->jumpRange = 0.;
	string enterHaze = object->haze ? object->haze->Name() : "";
	if(ImGui::InputText("haze", &enterHaze, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->haze = SpriteSet::Get(enterHaze);
		SetDirty();
	}

	if(ImGui::TreeNode("trades"))
	{
		index = 0;
		for(auto &&commodity : GameData::Commodities())
		{
			ImGui::PushID(index++);
			ImGui::Text("trade: %s", commodity.name.c_str());
			ImGui::SameLine();
			if(ImGui::InputInt("", &object->trade[commodity.name].base))
				SetDirty();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	bool openObjects = ImGui::TreeNode("objects");
	bool openAddObject = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Object"))
			openAddObject = true;
		ImGui::EndPopup();
	}
	if(openAddObject)
		ImGui::OpenPopup("Add Stellar Object##popup");

	if(ImGui::BeginPopupModal("Add Stellar Object##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static StellarObject object;

		static string planetName;
		ImGui::InputText("object", &planetName);

		static string spriteName;
		if(object.sprite)
			spriteName = object.sprite->Name();
		if(ImGui::InputText("sprite", &spriteName, ImGuiInputTextFlags_EnterReturnsTrue))
			object.sprite = SpriteSet::Get(spriteName);

		ImGui::InputInt("parent", &object.parent);
		ImGui::InputDoubleEx("distance", &object.distance);
		double period = 0.;
		if(object.speed)
			period = 360. / object.Speed();
		ImGui::InputDoubleEx("period", &period);
		object.speed = 360. / period;
		ImGui::InputDoubleEx("offset", &object.offset);


		if(ImGui::Button("Add"))
		{
			object.sprite = SpriteSet::Get(spriteName);
			object.planet = GameData::Planets().Find(planetName);
			this->object->objects.insert(this->object->objects.begin() + object.parent + 1, object);

			this->object->SetDate(editor.Player().GetDate());

			planetName.clear();
			spriteName.clear();
			object = {};

			if(this->object->objects.back().HasValidPlanet())
			{
				const Planet &planet = *this->object->objects.back().GetPlanet();
				if(!planet.IsWormhole() && planet.IsInhabited() && planet.IsAccessible(nullptr))
					this->object->attributes.erase("uninhabited");
			}
			SetDirty();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Cancel"))
		{
			planetName.clear();
			spriteName.clear();
			object = {};

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if(openObjects)
	{
		bool hovered = false;
		index = 0;
		int nested = 0;
		auto selected = object->objects.end();
		for(auto it = object->objects.begin(); it != object->objects.end(); ++it)
		{
			ImGui::PushID(index);
			RenderObject(*it, index, nested, hovered);
			if(hovered)
			{
				selected = it;
				hovered = false;
			}
			++index;
			ImGui::PopID();
		}
		ImGui::TreePop();

		if(selected != object->objects.end())
		{
			SetDirty();
			auto parent = selected->Parent();
			auto next = object->objects.erase(selected);
			size_t removed = 1;
			// Remove any child objects too.
			while(next != object->objects.end() && next->Parent() != parent)
			{
				next = object->objects.erase(next);
				++removed;
			}

			// Recalculate every parent index.
			for(auto it = next; it != object->objects.end(); ++it)
				if(it->Parent() != -1)
					it->parent -= removed;
		}
	}

	double arrival[2] = {object->extraHyperArrivalDistance, object->extraJumpArrivalDistance};
	if(ImGui::InputDouble2Ex("arrival", arrival))
		SetDirty();
	object->extraHyperArrivalDistance = arrival[0];
	object->extraJumpArrivalDistance = fabs(arrival[1]);
}



void SystemEditor::RenderObject(StellarObject &object, int index, int &nested, bool &hovered)
{
	bool isOpen = ImGui::TreeNode("object", "object %s", object.GetPlanet() ? object.Name().c_str() : "");

	ImGui::PushID(index);
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Remove"))
			hovered = true;
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if(isOpen)
	{
		static string spriteName;
		if(object.sprite)
			spriteName = object.sprite->Name();
		if(ImGui::InputText("sprite", &spriteName, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object.sprite = SpriteSet::Get(spriteName);
			SetDirty();
		}

		if(ImGui::InputDoubleEx("distance", &object.distance))
			SetDirty();
		double period = 360. / object.Speed();
		if(ImGui::InputDoubleEx("period", &period))
			SetDirty();
		object.speed = 360. / period;
		if(ImGui::InputDoubleEx("offset", &object.offset))
			SetDirty();

		if(IsDirty())
			this->object->SetDate(editor.Player().GetDate());

		if(index + 1 < static_cast<int>(this->object->objects.size()) && this->object->objects[index + 1].Parent() == index)
		{
			++nested; // If the next object is a child, don't close this tree node just yet.
			return;
		}
		else
			ImGui::TreePop();
	}

	// If are nested, then we need to remove this nesting until we are at the next desired level.
	if(nested && index + 1 >= static_cast<int>(this->object->objects.size()))
		while(nested--)
			ImGui::TreePop();
	else if(nested)
	{
		int nextParent = this->object->objects[index + 1].Parent();
		if(nextParent == object.Parent())
			return;
		while(nextParent != index)
		{
			nextParent = nextParent == -1 ? index : this->object->objects[nextParent].Parent();
			--nested;
			ImGui::TreePop();
		}
	}
}



void SystemEditor::WriteObject(DataWriter &writer, const System *system, const StellarObject *object)
{
	// Calculate the nesting of this object. We follow parent indices until we reach
	// the root node.
	int i = object->Parent();
	int nested = 0;
	while(i != -1)
	{
		i = system->objects[i].Parent();
		++nested;
	}

	for(i = 0; i < nested; ++i)
		writer.BeginChild();

	writer.WriteToken("object");
	if(object->GetPlanet())
		writer.WriteToken(object->Name());
	writer.Write();

	writer.BeginChild();
	if(object->HasSprite())
		writer.Write("sprite", object->GetSprite()->Name());
	writer.Write("distance", object->Distance());
	writer.Write("period", 360. / object->Speed());
	if(object->Offset())
		writer.Write("offset", object->Offset());
	writer.EndChild();

	for(i = 0; i < nested; ++i)
		writer.EndChild();
}



void SystemEditor::WriteToFile(DataWriter &writer, const System *system)
{
	// FIXME: Before saving, it would be great to diff the modified system to the one stored on file.
	// But unfortunately, the game doesn't save whether a system has been modified or not.
	// This means that after reloading a plugin that modifies an existing system, the game will
	// think that the modified system is the original one and we missave it.
	writer.Write("system", system->name);
	writer.BeginChild();

	writer.Write("pos", system->position.X(), system->position.Y());
	if(system->government)
		writer.Write("government", system->government->TrueName());
	if(!system->music.empty())
		writer.Write("music", system->music);
	if(system->links.empty())
		// If there are no links make sure that no other node adds some.
		writer.Write("remove", "link");
	for(auto &&link : system->links)
		writer.Write("link", link->Name());
	if(system->hidden)
		writer.Write("hidden");
	for(auto &&object : system->objects)
		WriteObject(writer, system, &object);
	for(auto &&asteroid : system->asteroids)
		writer.Write(asteroid.Type() ? "minables" : "asteroids", asteroid.Type() ? asteroid.Type()->Name() : asteroid.Name(), asteroid.Count(), asteroid.Energy());
	if(system->haze)
		writer.Write("haze", system->haze->Name());
	for(auto &&fleet : system->fleets)
		writer.Write("fleet", fleet.Get()->Name(), fleet.Period());
	for(auto &&hazard : system->hazards)
		writer.Write("hazard", hazard.Get()->Name(), hazard.Period());
	writer.Write("habitable", system->habitable);
	writer.Write("belt", system->asteroidBelt);
	if(system->jumpRange)
		writer.Write("jump range", system->jumpRange);
	if(system->extraHyperArrivalDistance == system->extraJumpArrivalDistance
			&& system->extraHyperArrivalDistance)
		writer.Write("arrival", system->extraHyperArrivalDistance);
	else if(system->extraHyperArrivalDistance != system->extraJumpArrivalDistance)
	{
		writer.Write("arrival");
		writer.BeginChild();
		if(system->extraHyperArrivalDistance)
			writer.Write("link", system->extraHyperArrivalDistance);
		if(system->extraJumpArrivalDistance)
			writer.Write("jump", system->extraJumpArrivalDistance);
		writer.EndChild();
	}
	for(auto &&trade : system->trade)
		writer.Write("trade", trade.first, trade.second.base);

	if(!system->attributes.empty() && (system->attributes.size() > 1 || *system->attributes.begin() != "uninhabited"))
	{
		writer.WriteToken("attributes");
		for(auto &&attribute : system->attributes)
			if(attribute != "uninhabited")
				writer.WriteToken(attribute);
		writer.Write();
	}

	writer.EndChild();
}



void SystemEditor::UpdateMap(bool updateSystem) const
{
	if(updateSystem)
		GameData::UpdateSystems();
	if(auto *mapPanel = dynamic_cast<MapPanel*>(editor.GetUI().Top().get()))
	{
		mapPanel->UpdateCache();
		mapPanel->distance = DistanceMap(editor.Player());
	}
	if(auto map = mapEditor.lock())
		map->UpdateCache();
}
