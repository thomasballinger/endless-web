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
#include "Imgui.h"
#include "imgui_internal.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

using namespace std;



SystemEditor::SystemEditor(Editor &editor, bool &show) noexcept
	: editor(editor), showSystemMenu(show)
{
}



const vector<System> &SystemEditor::Systems() const
{
	return systems;
}



void SystemEditor::Render()
{
	if(dirty.count(system))
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("System Editor", &showSystemMenu, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if(dirty.count(system))
		ImGui::PopStyleColor(3);

	if(ImGui::InputText("system", &searchBox))
		if(auto *ptr = GameData::Systems().Find(searchBox))
		{
			system = const_cast<System *>(ptr);
			searchBox.clear();
		}
	if(!system)
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	ImGui::SameLine();
	if(system && searchBox.empty())
		ImGui::PushDisabled();
	bool clone = ImGui::Button("Clone");
	if(searchBox.empty() || !system)
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(searchBox.empty())
				ImGui::SetTooltip("Input the new name for the system above.");
			else if(!system)
				ImGui::SetTooltip("Select a system first.");
		}
	}
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

	if(!system)
	{
		ImGui::End();
		return;
	}

	if(reset)
	{
		*system = *GameData::defaultSystems.Get(system->name);
		for(auto &&link : system->links)
			const_cast<System *>(link)->Link(system);
		UpdateMap();
		dirty.erase(system);
	} 
	if(clone)
	{
		auto *clone = const_cast<System *>(GameData::Systems().Get(searchBox));
		*clone = *system;
		system = clone;

		system->name = searchBox;
		system->position += Point(100., 0.);
		system->objects.clear();
		system->links.clear();
		system->attributes.insert("uninhabited");
		GameData::UpdateSystems();
		for(auto &&link : editor.Player().visitedSystems)
			editor.Player().Visit(*link);
		UpdateMap(/*updateSystem=*/false);
		searchBox.clear();
		dirty.insert(system);
	}
	if(save)
		WriteToPlugin(system);

	ImGui::Separator();
	ImGui::Spacing();
	RenderSystem();
	ImGui::End();
}



void SystemEditor::RenderSystem()
{
	int index = 0;

	ImGui::Text("name: %s", system->name.c_str());
	if(ImGui::Checkbox("hidden", &system->hidden))
		dirty.insert(system);

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : system->attributes)
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
			system->attributes.insert(attribute);
		for(auto &&attribute : toRemove)
			system->attributes.erase(attribute);
		if(!toAdd.empty() || !toRemove.empty())
			dirty.insert(system);

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##system", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			system->attributes.insert(move(addAttribute));
			dirty.insert(system);
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("links"))
	{
		set<System *> toAdd;
		set<System *> toRemove;
		index = 0;
		for(auto &link : system->links)
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
			system->Link(sys);
			WriteToPlugin(sys);
			dirty.insert(sys);
		}
		for(auto &&sys : toRemove)
		{
			system->Unlink(sys);
			WriteToPlugin(sys);
			dirty.insert(sys);
		}
		if(!toAdd.empty() || !toRemove.empty())
		{
			dirty.insert(system);
			UpdateMap();
		}
		ImGui::TreePop();
	}

	bool asteroidsOpen = ImGui::TreeNode("asteroids");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Asteroid"))
		{
			system->asteroids.emplace_back("small rock", 1, 1.);
			dirty.insert(system);
		}
		if(ImGui::Selectable("Add Mineable"))
		{
			system->asteroids.emplace_back(&GameData::Minables().begin()->second, 1, 1.);
			dirty.insert(system);
		}
		ImGui::EndPopup();
	}

	if(asteroidsOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &asteroid : system->asteroids)
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
								dirty.insert(system);
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
						dirty.insert(system);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputDoubleEx("##energy", &asteroid.energy))
						dirty.insert(system);
					ImGui::TreePop();
				}
			}
			else
			{
				bool open = ImGui::TreeNode("asteroids");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = index;
					ImGui::EndPopup();
				}

				if(open)
				{
					ImGui::SetNextItemWidth(300.f);
					if(ImGui::InputText("##asteroids", &asteroid.name))
						dirty.insert(system);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputInt("##count", &asteroid.count))
						dirty.insert(system);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if(ImGui::InputDoubleEx("##energy", &asteroid.energy))
						dirty.insert(system);
					ImGui::TreePop();
				}
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			system->asteroids.erase(system->asteroids.begin() + toRemove);
			dirty.insert(system);
		}
		ImGui::TreePop();
	}

	bool fleetOpen = ImGui::TreeNode("fleets");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Fleet"))
			system->fleets.emplace_back(&GameData::Fleets().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(fleetOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &fleet : system->fleets)
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
							dirty.insert(system);
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if(ImGui::InputInt("##period", &fleet.period))
					dirty.insert(system);
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			system->fleets.erase(system->fleets.begin() + toRemove);
			dirty.insert(system);
		}
		ImGui::TreePop();
	}

	bool hazardOpen = ImGui::TreeNode("hazards");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Hazard"))
			system->hazards.emplace_back(&GameData::Hazards().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(hazardOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &hazard : system->hazards)
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
							dirty.insert(system);
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				if(ImGui::InputInt("##period", &hazard.period))
					dirty.insert(system);
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			system->hazards.erase(system->hazards.begin() + toRemove);
			dirty.insert(system);
		}
		ImGui::TreePop();
	}

	double pos[2] = {system->position.X(), system->Position().Y()};
	bool updatedPos = false;
	if(ImGui::InputDouble2Ex("pos", pos, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		system->position.Set(pos[0], pos[1]);
		updatedPos = true;
	}
	ImGui::SameLine();
	auto *mapPanel = dynamic_cast<MapPanel *>(editor.GetUI().Top().get());
	if(!mapPanel)
		ImGui::PushDisabled();
	if(ImGui::Button("set to last click pos"))
	{
		system->position = mapPanel->click;
		updatedPos = true;
	}
	if(!mapPanel)
		ImGui::PopDisabled();
	if(updatedPos)
	{
		mapPanel->UpdateCache();
		dirty.insert(system);
	}

	{
		if(ImGui::BeginCombo("governments", system->government ? system->government->GetName().c_str() : ""))
		{
			for(const auto &government : GameData::Governments())
			{
				const bool selected = &government.second == system->government;
				if(ImGui::Selectable(government.first.c_str(), selected))
				{
					system->government = &government.second;
					UpdateMap(/*updateSystems=*/false);
					dirty.insert(system);
				}

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	ImGui::InputText("music", &system->music);

	ImGui::InputDoubleEx("habitable", &system->habitable);
	ImGui::InputDoubleEx("belt", &system->asteroidBelt);
	ImGui::InputDoubleEx("jump range", &system->jumpRange);
	if(system->jumpRange < 0.)
		system->jumpRange = 0.;
	string enterHaze = system->haze ? system->haze->Name() : "";
	if(ImGui::InputText("haze", &enterHaze, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		system->haze = SpriteSet::Get(enterHaze);
		dirty.insert(system);
	}

	if(ImGui::TreeNode("trades"))
	{
		index = 0;
		for(auto &&commodity : GameData::Commodities())
		{
			ImGui::PushID(index++);
			ImGui::Text("trade: %s", commodity.name.c_str());
			ImGui::SameLine();
			if(ImGui::InputInt("", &system->trade[commodity.name].base))
				dirty.insert(system);
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
		static string planet;
		static int parent = -1;
		static double distance;
		static double period;
		static double offset;
		ImGui::InputText("object", &planet);

		static string spriteName;
		static float frameRate = 2.f / 60.f;
		static double delay;
		static bool randomStartFrame;
		static bool noRepeat;
		static bool rewind;
		if(ImGui::TreeNode("sprite"))
		{
			ImGui::InputText("sprite", &spriteName);
			ImGui::InputFloatEx("frame rate", &frameRate);
			ImGui::InputDoubleEx("delay", &delay);
			ImGui::Checkbox("random start frame", &randomStartFrame);
			ImGui::Checkbox("no repeat", &noRepeat);
			ImGui::Checkbox("rewind", &rewind);

			ImGui::TreePop();
		}

		ImGui::InputInt("parent", &parent);
		ImGui::InputDoubleEx("distance", &distance);
		ImGui::InputDoubleEx("period", &period);
		ImGui::InputDoubleEx("offset", &offset);

		if(ImGui::Button("Add"))
		{
			StellarObject object;
			object.parent = parent;
			object.distance = distance;
			object.speed = 360. / period;
			object.offset = offset;
			object.sprite = SpriteSet::Get(spriteName);
			object.frameRate = frameRate / 60.f;
			object.delay = delay;
			object.randomize = randomStartFrame;
			object.repeat = !noRepeat;
			object.rewind = rewind;
			object.planet = GameData::Planets().Find(planet);
			system->objects.insert(system->objects.begin() + parent + 1, object);

			planet.clear();
			parent = -1;
			distance = 0.;
			period = 0.;
			offset = 0.;
			spriteName.clear();
			frameRate = 2.f / 60.f;
			delay = 0.;
			randomStartFrame = false;
			noRepeat = false;
			rewind = false;

			if(system->objects.back().HasValidPlanet())
			{
				const Planet &planet = *system->objects.back().GetPlanet();
				if(!planet.IsWormhole() && planet.IsInhabited() && planet.IsAccessible(nullptr))
					system->attributes.erase("uninhabited");
			}
			dirty.insert(system);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Cancel"))
		{
			planet.clear();
			parent = -1;
			distance = 0.;
			period = 0.;
			offset = 0.;
			spriteName.clear();
			frameRate = 2.f / 60.f;
			delay = 0.;
			randomStartFrame = false;
			noRepeat = false;
			rewind = false;

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if(openObjects)
	{
		bool hovered = false;
		index = 0;
		int nested = 0;
		auto selected = system->objects.end();
		for(auto it = system->objects.begin(); it != system->objects.end(); ++it)
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

		if(selected != system->objects.end())
		{
			dirty.insert(system);
			auto parent = selected->Parent();
			auto next = system->objects.erase(selected);
			size_t removed = 1;
			// Remove any child objects too.
			while(next != system->objects.end() && next->Parent() != parent)
			{
				next = system->objects.erase(next);
				++removed;
			}

			// Recalculate every parent index.
			for(auto it = next; it != system->objects.end(); ++it)
				if(it->Parent() != -1)
					it->parent -= removed;
		}
	}

	double arrival[2] = {system->extraHyperArrivalDistance, system->extraJumpArrivalDistance};
	if(ImGui::InputDouble2Ex("arrival", arrival))
		dirty.insert(system);
	system->extraHyperArrivalDistance = arrival[0];
	system->extraJumpArrivalDistance = fabs(arrival[1]);
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
		if(ImGui::InputDoubleEx("distance", &object.distance))
			dirty.insert(system);
		double period = 360. / object.Speed();
		if(ImGui::InputDoubleEx("period", &period))
			dirty.insert(system);
		object.speed = 360. / period;
		if(ImGui::InputDoubleEx("offset", &object.offset))
			dirty.insert(system);

		if(dirty.count(system))
			system->SetDate(editor.Player().GetDate());

		if(index + 1 < system->objects.size() && system->objects[index + 1].Parent() == index)
			return (void)++nested; // If the next object is a child, don't close this tree node just yet.
		else
			ImGui::TreePop();
	}

	// If are nested, then we need to remove this nesting until we are at the next desired level.
	if(nested && index + 1 >= system->objects.size())
		while(nested--)
			ImGui::TreePop();
	else if(nested)
	{
		int nextParent = system->objects[index + 1].Parent();
		if(nextParent == object.Parent())
			return;
		while(nextParent != index)
		{
			nextParent = nextParent == -1 ? index : system->objects[nextParent].Parent();
			--nested;
			ImGui::TreePop();
		}
	}
}



void SystemEditor::WriteObject(DataWriter &writer, const StellarObject *object)
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



void SystemEditor::WriteToPlugin(const System *system)
{
	if(!editor.HasPlugin())
		return;

	dirty.erase(system);
	for(auto &&sys : systems)
		if(system->name == sys.name)
		{
			sys = *system;
			return;
		}

	systems.push_back(*system);
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
		writer.Write("government", system->government->GetName());
	if(!system->music.empty())
		writer.Write("music", system->music);
	for(auto &&link : system->links)
		writer.Write("link", link->Name());
	if(system->hidden)
		writer.Write("hidden");
	for(auto &&object : system->objects)
		WriteObject(writer, &object);
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

	if(!system->attributes.empty())
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
}
