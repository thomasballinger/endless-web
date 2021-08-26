/* imgui_ex.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "imgui_ex.h"

#include "imgui_internal.h"
#include "imgui_stdlib.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>



namespace ImGui
{
	IMGUI_API bool InputDoubleEx(const char *label, double *v, ImGuiInputTextFlags flags)
	{
		return InputDouble(label, v, 0., 0., "%g", flags);
	}



	IMGUI_API bool InputFloatEx(const char *label, float *v, ImGuiInputTextFlags flags)
	{
		return InputFloat(label, v, 0., 0., "%g", flags);
	}



	IMGUI_API bool InputDouble2Ex(const char *label, double *v, ImGuiInputTextFlags flags)
	{
		return InputScalarN(label, ImGuiDataType_Double, v, 2, nullptr, nullptr, "%g", flags);
	}



	IMGUI_API bool InputInt64Ex(const char *label, int64_t *v, ImGuiInputTextFlags flags)
	{
		return InputScalar(label, ImGuiDataType_S64, v, nullptr, nullptr, "%" PRId64, flags);
	}



	IMGUI_API bool InputSizeTEx(const char *label, size_t *v, ImGuiInputTextFlags flags)
	{
		return InputScalar(label, ImGuiDataType_U64, v, nullptr, nullptr, "%zu", flags);
	}



	IMGUI_API bool InputSwizzle(const char *label, int *swizzle, bool allowNoSwizzle)
	{
		constexpr int count = 29;
		constexpr const char *swizzles[count] =
		{
			"0 - red + yellow markings",
			"1 - red + magenta markings",
			"2 - green + yellow",
			"3 - green + cyan",
			"4 - blue + magenta",
			"5 - blue + cyan",
			"6 - red + black",
			"7 - pure red",
			"8 - faded red",
			"9 - pure black",
			"10 - faded black",
			"11 - pure white",
			"12 - darkened blue",
			"13 - pure blue",
			"14 - faded blue",
			"15 - darkened cyan",
			"16 - pure cyan",
			"17 - faded cyan",
			"18 - darkened green",
			"19 - pure green",
			"20 - faded green",
			"21 - darkened yellow",
			"22 - pure yellow",
			"23 - faded yellow",
			"24 - darkened magenta",
			"25 - pure magenta",
			"26 - faded magenta",
			"27 - red only (cloaked)",
			"28 - black only (outline)",
		};
		bool changed = false;
		if(BeginCombo(label, *swizzle == -1 ? "-1 - none" : swizzles[*swizzle]))
		{
			for(int i = allowNoSwizzle ? -1 : 0; i < count; ++i)
			{
				const bool selected = i == *swizzle;
				if(Selectable(i == -1 ? "-1 - none" : swizzles[i], selected))
				{
					*swizzle = i;
					changed = true;
				}
				if(selected)
					SetItemDefaultFocus();
			}
			EndCombo();
		}
		return changed;
	}
}
