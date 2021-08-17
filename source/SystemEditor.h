/* SystemEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SYSTEM_EDITOR_H_
#define SYSTEM_EDITOR_H_

#include "System.h"
#include "TemplateEditor.h"

#include <memory>
#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;
class MapEditorPanel;
class StellarObject;



// Class representing the system editor window.
class SystemEditor : public TemplateEditor<System> {
public:
	SystemEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const System *system);

	// Updates the given system's position by the given delta.
	void UpdateSystemPosition(const System *system, Point dp);


private:
	void RenderSystem();
	void RenderObject(StellarObject &object, int index, int &nested, bool &hovered);

	void WriteObject(DataWriter &writer, const System *system, const StellarObject *object);

	void UpdateMap(bool updateSystem = true) const;


private:
	std::weak_ptr<MapEditorPanel> mapEditor;
};



#endif
