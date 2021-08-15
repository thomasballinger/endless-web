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

#include "DataWriter.h"
#include "Ship.h"
#include "System.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

class Editor;
class StellarObject;



// Class representing the system editor window.
class SystemEditor {
public:
	SystemEditor(Editor &editor, bool &show) noexcept;
	SystemEditor(const SystemEditor &) = delete;
	SystemEditor& operator=(const SystemEditor &) = delete;

	const std::vector<System> &Systems() const;
	void Render();
	void WriteToFile(DataWriter &writer, const System *system);
	void WriteToPlugin(const System *system);


private:
	void RenderSystem();
	void RenderObject(StellarObject &object, int index, int &nested, bool &hovered);

	void WriteObject(DataWriter &writer, const StellarObject *object);

	void UpdateMap(bool updateSystem = true) const;


private:
	Editor &editor;
	bool &showSystemMenu;

	std::string searchBox;
	System *system = nullptr;

	std::set<const System *> dirty;
	std::vector<System> systems;
	std::vector<std::string> originalSystems;
};



#endif
