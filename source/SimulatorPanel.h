/* SimulatorPanel.h
Copyright (c) 2026 by Thomas Ballinger

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "Panel.h"

#include "CodeDocument.h"
#include "CodeEditor.h"
#include "GuidanceSimulation.h"
#include "MissileGuidance.h"
#include "Point.h"
#include "Rectangle.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Outfit;
class PlayerInfo;
class Ship;



// A practice range for guidance scripts, set apart from the game itself: a
// grid instead of stars and planets, where a launcher fires a homing weapon
// at a target flying a drill. The weapon's guidance script is open in an
// editor beside it. Every change that compiles is checked against what the
// script in use did: if it would have steered any projectile differently, the
// range rewinds to the first step where it would have, and goes on from there
// with the new script, with the old run's trails shown faintly beside the new
// ones. Mistakes are marked on their lines instead. Time can be paused,
// stepped, sped up, and restarted, and clicking a projectile shows, in the
// editor, which lines of its script ran on the latest step and what they
// produced.
class SimulatorPanel : public Panel {
public:
	// With a player, this is the missile workshop: saving installs the
	// program on the player's ships, rather than in a plugin.
	SimulatorPanel(const Outfit &outfit, const Ship &target, PlayerInfo *player = nullptr);

	virtual void Step() override;
	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool TextInput(const std::string &text) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Release(int x, int y, MouseButton button) override;
	virtual bool Scroll(double dx, double dy) override;


private:
	// Compile the editor's text, and if it compiles, use it. If it would have
	// steered differently than the script in use, rewind to where it first
	// would have, and go on from there with it.
	void Recompile();
	// Show the next frame of a rewind, or end it at once.
	void StepRewind();
	void FinishRewind();
	// Start over, forgetting the last edit's rewind and old trails.
	void Restart();
	// Show the selected projectile's latest run in the editor.
	void UpdateRunNotes();
	void Save();
	// The weapon's own guidance, as text.
	std::string FactoryText() const;
	// Fire at the next of the practice targets.
	void NextTarget();
	void Layout();

	// Convert between positions in the simulation and on the screen.
	Point ToScreen(const Point &position) const;
	Point ToWorld(const Point &screen) const;
	void DrawGrid() const;
	void DrawTrails() const;
	void DrawGhosts() const;
	void DrawTimeline() const;
	void DrawSelection() const;
	void DrawInspector() const;
	void DrawJamming() const;
	// Rewind and play symbols over the range, like a tape deck's.
	void DrawTapeDeck() const;
	// The timeline's ends, and its height on the screen.
	double TimelineLeft() const;
	double TimelineRight() const;
	double TimelineY() const;
	// Replay to the step under the given point of the timeline.
	void ScrubTo(double x);
	void DrawCloseUp(const GuidanceSimulation::Missile &missile, const Rectangle &box) const;
	// The list of what a program can use, starting at the given height.
	void DrawReference(const GuidanceSimulation::Missile &missile, double y) const;
	// Follow the next (or previous) missile in flight.
	void CycleMissile(int direction);
	// Keep following one missile until it is gone, and then the next.
	void UpdateFollow();
	// Fly the launcher with the arrow keys.
	void FlyLauncher();
	// What confused a missile's seeker, e.g. "the Carrier's radar jamming".
	std::string Blame(const GuidanceSimulation::Missile &missile) const;
	void DrawStatus() const;


private:
	const Outfit &outfit;
	PlayerInfo *player = nullptr;
	std::unique_ptr<GuidanceSimulation> simulation;
	size_t targetIndex = 0;

	// The script being edited, the version of it in use, and the last
	// version of the text that was compiled.
	CodeDocument document;
	CodeEditor editor;
	MissileGuidance guidance;
	// The script in use before the last edit, which a rewind shows going back.
	MissileGuidance previousGuidance;
	std::string guidanceName;
	uint64_t compiledVersion = 0;
	// The document's version on the last frame, and frames since it changed.
	uint64_t seenVersion = 0;
	int quietFrames = 0;
	// Whether the text has mistakes, so an older version is still running.
	bool hasErrors = false;
	std::string status;
	int statusTime = 0;
	// Where the script came from, and where saving it puts it.
	std::filesystem::path sourcePath;

	// Where the last edit first mattered, and the trails from before it, which
	// are shown faintly until the new run catches up with where the old one was.
	GuidanceSimulation::Difference difference;
	std::vector<std::deque<Point>> ghosts;
	int ghostStep = -1;
	// A rewind in progress: from which step to which, and its frames.
	int rewindFrom = 0;
	int rewindTo = 0;
	int rewindFrame = 0;
	int rewindFrames = 0;
	// After a rewind, how many more frames to hold at the step it went back
	// to, and then how many to show that the new run is playing.
	int holdFrames = 0;
	int playFrames = 0;
	// Whether the timeline is being dragged.
	bool isScrubbing = false;
	// Whether the arrow keys fly the target (rather than the launcher).
	bool flyTarget = false;
	// Whether the arrow keys were flying the launcher on the last step.
	bool wasFlying = false;
	// Where each name in the list of what a program can use was drawn, the
	// text clicking it types, and its value if it is a vector.
	class Reference {
	public:
		Rectangle box;
		std::string text;
		bool isVector = false;
		Point vector;
	};
	mutable std::vector<Reference> references;

	// Whether a key gave the editor the keyboard on this frame, so that the
	// text of that same key press isn't typed into it.
	bool justFocused = false;

	bool paused = false;
	// Steps per frame: 1/4, 1/2, 1, 2, 4 or 8.
	int speedIndex = 2;
	int frame = 0;
	// Frames drawn, for things that flicker.
	int frameCount = 0;

	// The part of the screen showing the range.
	Rectangle view;
	Point center;
	double zoom = .5;
	// The zoom chosen with the mouse wheel, relative to the automatic one.
	double zoomFactor = 1.;
	// Where a drag in the editor is.
	Point dragPoint;
	bool isDragging = false;
};
