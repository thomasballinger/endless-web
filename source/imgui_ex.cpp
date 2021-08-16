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
		return InputScalar(label, ImGuiDataType_S64, &v, nullptr, nullptr, nullptr, flags);
	}
}