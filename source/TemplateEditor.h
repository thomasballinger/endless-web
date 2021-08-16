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

#include <set>
#include <string>
#include <list>

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
	void WriteToPlugin(const T *object)
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


protected:
	Editor &editor;
	bool &show;

	std::string searchBox;
	T *object = nullptr;


private:
	std::set<const T *> dirty;
	std::list<T> changes;
};



#endif
