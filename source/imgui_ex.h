/* imgui_ex.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef IMGUI_EX_H_
#define IMGUI_EX_H_

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Set.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>



namespace ImGui
{
	IMGUI_API bool InputDoubleEx(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputFloatEx(const char *label, float *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputDouble2Ex(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputInt64Ex(const char *label, int64_t *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputSizeTEx(const char *label, size_t *v, ImGuiInputTextFlags flags = 0);

	template <typename T>
	IMGUI_API bool InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements);
}



template <typename T>
IMGUI_API bool ImGui::InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements)
{
	ImGuiWindow *window = GetCurrentWindow();
	const auto callback = [](ImGuiInputTextCallbackData *data)
	{
		bool &autocomplete = *reinterpret_cast<bool *>(data->UserData);
		switch(data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
			autocomplete = true;
			break;
		}

		return 0;
	};

	const auto id = ImHashStr("##combo/popup", 0, window->GetID(label));
	bool autocomplete = false;
	bool enter = false;
	if(InputText(label, input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, callback, &autocomplete))
	{
		SetActiveID(0, FindWindowByID(id));
		enter = true;
	}

	bool isOpen = IsPopupOpen(id, ImGuiPopupFlags_None);
	if(IsItemActive() && !isOpen)
	{
		OpenPopupEx(id, ImGuiPopupFlags_None);
		isOpen = true;
	}

	if(!isOpen)
		return false;
	if(autocomplete && input->empty())
		autocomplete = false;

    const ImRect bb(window->DC.CursorPos,
			window->DC.CursorPos + ImVec2(CalcItemWidth(), 0.f));

	bool changed = false;
	if(BeginComboPopup(id, bb, ImGuiComboFlags_None, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		BringWindowToDisplayFront(GetCurrentWindow());
		if(enter)
		{
			CloseCurrentPopup();
			EndCombo();
			return false;
		}

		std::vector<const std::string *> strings(elements.size());
		std::transform(elements.begin(), elements.end(), strings.begin(),
				[](const std::pair<const std::string, T> &pair) { return &pair.first; });

		std::vector<std::pair<double, const char *>> weights;
		for(auto &&second : strings)
		{
			const auto generatePairs = [](const std::string &str)
			{
				std::vector<std::pair<char, char>> pair;
				for(int i = 0; i < static_cast<int>(str.size()); ++i)
					pair.emplace_back(str[i], i + 1 < static_cast<int>(str.size()) ? str[i + 1] : '\0');
				return pair;
			};
			std::vector<std::pair<char, char>> lhsPairs = generatePairs(*input);
			std::vector<std::pair<char, char>> rhsPairs = generatePairs(*second);

			int sameCount = 0;
			const auto transform = [](std::pair<char, char> c)
			{
				if(std::isalpha(c.first))
					c.first = std::tolower(c.first);
				if(std::isalpha(c.second))
					c.second = std::tolower(c.second);
				return c;
			};
			for(int i = 0, size = std::min(lhsPairs.size(), rhsPairs.size()); i < size; ++i)
				if(transform(lhsPairs[i]) == transform(rhsPairs[i]))
					++sameCount;

			weights.emplace_back((2. * sameCount) / (lhsPairs.size() + rhsPairs.size()), second->c_str());
		}

		std::sort(weights.begin(), weights.end(),
				[](const std::pair<double, const char *> &lhs, const std::pair<double, const char *> &rhs)
					{ return lhs.first > rhs.first; });
		auto topWeight = weights[0].first;
		for(const auto &item : weights)
		{
			if(topWeight && item.first < topWeight * .45)
				break;
			if(Selectable(item.second) || autocomplete)
			{
				*element = const_cast<T *>(elements.Get(item.second));
				changed = true;
				*input = item.second;
				if(autocomplete)
				{
					autocomplete = false;
					CloseCurrentPopup();
					SetActiveID(0, GetCurrentWindow());
				}
			}
		}
		EndCombo();
	}

	return changed;
}



#endif
