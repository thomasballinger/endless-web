/* EffectEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "EffectEditor.h"

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

#include <cassert>
#include <map>

using namespace std;



EffectEditor::EffectEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Effect>(editor, show)
{
}



void EffectEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Effect Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::InputCombo("effect", &searchBox, &object, GameData::Effects()))
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
				ImGui::SetTooltip("Select an effect first.");
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
				ImGui::SetTooltip("Input the new name for the effect above.");
			else if(!object)
				ImGui::SetTooltip("Select a effect first.");
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
				ImGui::SetTooltip("Select an effect first.");
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
		*object = *GameData::defaultEffects.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Effect *>(GameData::Effects().Get(searchBox));
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
	RenderEffect();
	ImGui::End();
}



void EffectEditor::RenderEffect()
{
	ImGui::Text("effect: %s", object->name.c_str());
	RenderElement(object, "sprite");

	string soundName = object->sound ? object->sound->Name() : "";
	if(ImGui::InputCombo("sound", &soundName, &object->sound, Audio::GetSounds()))
		SetDirty();

	if(ImGui::InputInt("lifetime", &object->lifetime))
		SetDirty();
	if(ImGui::InputInt("random lifetime", &object->randomLifetime))
		SetDirty();
	if(ImGui::InputDoubleEx("velocity scale", &object->velocityScale))
		SetDirty();
	if(ImGui::InputDoubleEx("random velocity", &object->randomVelocity))
		SetDirty();
	if(ImGui::InputDoubleEx("random angle", &object->randomAngle))
		SetDirty();
	if(ImGui::InputDoubleEx("random spin", &object->randomSpin))
		SetDirty();
	if(ImGui::InputDoubleEx("random frame rate", &object->randomFrameRate))
		SetDirty();
}



void EffectEditor::WriteToFile(DataWriter &writer, const Effect *effect)
{
	writer.Write("effect", effect->name);
	writer.BeginChild();

	if(effect->HasSprite())
	{
		writer.Write("sprite", effect->GetSprite()->Name());
		writer.BeginChild();
		if(effect->frameRate != 2.f / 60.f)
			writer.Write("frame rate", effect->frameRate * 60.f);
		if(effect->delay)
			writer.Write("delay", effect->delay);
		if(effect->randomize)
			writer.Write("random start frame");
		if(!effect->repeat)
			writer.Write("no repeat");
		if(effect->rewind)
			writer.Write("rewind");
		writer.EndChild();
	}

	if(effect->sound)
		writer.Write("sound", effect->sound->Name());
	if(effect->lifetime)
		writer.Write("lifetime", effect->lifetime);
	if(effect->randomLifetime)
		writer.Write("random lifetime", effect->randomLifetime);
	if(effect->velocityScale != 1.)
		writer.Write("velocity scale", effect->velocityScale);
	if(effect->randomAngle)
		writer.Write("random angle", effect->randomAngle);
	if(effect->randomSpin)
		writer.Write("random spin", effect->randomSpin);
	if(effect->randomFrameRate)
		writer.Write("random framerate", effect->randomFrameRate);
	writer.EndChild();
}
