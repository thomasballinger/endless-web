/* OutfitEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef OUTFIT_EDITOR_H_
#define OUTFIT_EDITOR_H_

#include "Outfit.h"
#include "TemplateEditor.h"

#include <map>
#include <string>
#include <vector>

class DataWriter;
class Editor;



// Class representing the outfit editor window.
class OutfitEditor : public TemplateEditor<Outfit> {
public:
	OutfitEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Outfit *outfit);

private:
	void RenderOutfitMenu();
	void RenderOutfit();
};



#endif
