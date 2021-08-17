/* MapEditorPanel.h
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef MAP_EDITOR_PANEL_H_
#define MAP_EDITOR_PANEL_H_

#include "Panel.h"

#include "Color.h"
#include "Point.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class Angle;
class Government;
class PlayerInfo;
class System;
class SystemEditor;



// This class provides a limited version of the MapPanel which you can edit.
class MapEditorPanel : public Panel {
public:
	explicit MapEditorPanel(PlayerInfo &player, SystemEditor *systemEditor);

	virtual void Step() override;
	virtual void Draw() override;

	// Map panels allow fast-forward to stay active.
	virtual bool AllowFastForward() const override;

	const System *Selected() const;


protected:
	// Only override the ones you need; the default action is to return false.
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, int clicks) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Scroll(double dx, double dy) override;
	virtual bool Release(int x, int y) override;

	// Get the color mapping for various system attributes.
	static Color MapColor(double value);
	static Color GovernmentColor(const Government *government);
	static Color UninhabitedColor();

	void Select(const System *system);
	void Find(const std::string &name);

	double Zoom() const;

	// Function for the "find" dialogs:
	static int Search(const std::string &str, const std::string &sub);


protected:
	PlayerInfo &player;
	SystemEditor *systemEditor;

	// The (non-null) system which is currently selected.
	std::vector<const System *> selectedSystems;

	double playerJumpDistance;

	Point center;
	Point recenterVector;
	int recentering = 0;

	// Center the view on the given system (may actually be slightly offset
	// to account for panels on the screen).
	void CenterOnSystem(bool immediate = false);

	// Cache the map layout, so it doesn't have to be re-calculated every frame.
	// The cache must be updated when the coloring mode changes.
	void UpdateCache();


private:
	void DrawWormholes();
	void DrawLinks();
	// Draw systems in accordance to the set commodity color scheme.
	void DrawSystems();
	void DrawNames();


private:
	std::vector<std::pair<Point, Point>> links;
	Point click;
	bool isDragging = false;
	bool moveSystems = false;

	friend class SystemEditor;
};



#endif
