/* TemplateEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef TEMPLATE_EDITOR_H_
#define TEMPLATE_EDITOR_H_

#include "Audio.h"
#include "Body.h"
#include "Effect.h"
#include "GameData.h"
#include "Sound.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_stdlib.h"

#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

class Editor;



// Base class common for any editor window.
template <typename T>
class TemplateEditor {
public:
	TemplateEditor(Editor &editor, bool &show) noexcept
		: editor(editor), show(show) {}
	TemplateEditor(const TemplateEditor &) = delete;
	TemplateEditor& operator=(const TemplateEditor &) = delete;

	const std::list<T> &Changes() const { return changes; }
	const std::set<const T *> &Dirty() const { return dirty; }

	// Saves the specified object.
	void WriteToPlugin(const T *object) { WriteToPlugin(object, 0); }
	template<typename U>
	void WriteToPlugin(const U *object, ...)
	{
		dirty.erase(object);
		for(auto &&obj : changes)
			if(obj.Name() == object->Name())
			{
				obj = *object;
				return;
			}
		changes.push_back(*object);
	}
	template <typename U>
	void WriteToPlugin(const U *object, typename std::decay<decltype(std::declval<U>().TrueName())>::type *)
	{
		dirty.erase(object);
		for(auto &&obj : changes)
			if(obj.TrueName() == object->TrueName())
			{
				obj = *object;
				return;
			}
		changes.push_back(*object);
	}
	// Saves every unsaved object.
	void WriteAll()
	{
		auto copy = dirty;
		for(auto &&obj : copy)
			WriteToPlugin(obj);
	}

protected:
	// Marks the current object as dirty.
	void SetDirty() { dirty.insert(object); }
	void SetDirty(const T *obj) { dirty.insert(obj); }
	bool IsDirty() { return dirty.count(object); }
	void SetClean() { dirty.erase(object); }

	void RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map);
	bool RenderElement(Body *sprite, const std::string &name);
	void RenderSound(const std::string &name, std::map<const Sound *, int> &map);
	void RenderEffect(const std::string &name, std::map<const Effect *, int> &map);


protected:
	Editor &editor;
	bool &show;

	std::string searchBox;
	T *object = nullptr;


private:
	std::set<const T *> dirty;
	std::list<T> changes;
};



template <typename T>
void TemplateEditor<T>::RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map)
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



template <typename T>
bool TemplateEditor<T>::RenderElement(Body *sprite, const std::string &name)
{
	static std::string spriteName;
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
		if(ImGui::InputCombo("sprite", &spriteName, &sprite->sprite, SpriteSet::GetSprites()))
			SetDirty();

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



template <typename T>
void TemplateEditor<T>::RenderSound(const std::string &name, std::map<const Sound *, int> &map)
{
	static std::string soundName;
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
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			soundName = it->first ? it->first->Name() : "";
			if(ImGui::InputCombo("##sound", &soundName, &toAdd, Audio::GetSounds()))
			{
				toRemove = it;
				SetDirty();
			}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}

		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



template <typename T>
void TemplateEditor<T>::RenderEffect(const std::string &name, std::map<const Effect *, int> &map)
{
	static std::string effectName;
	effectName.clear();

	if(ImGui::TreeNode(name.c_str()))
	{
		Effect *toAdd = nullptr;
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			effectName = it->first->Name();
			if(ImGui::InputCombo("##effect", &effectName, &toAdd, GameData::Effects()))
			{
				toRemove = it;
				SetDirty();
			}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}

		static std::string newEffectName;
		static const Effect *newEffect;
		if(ImGui::InputCombo("new effect", &newEffectName, &newEffect, GameData::Effects()))
			if(!newEffectName.empty())
			{
				++map[newEffect];
				newEffectName.clear();
				newEffect = nullptr;
				SetDirty();
			}
		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		else if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



#endif
