/* Personality.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef PERSONALITY_H_
#define PERSONALITY_H_

#include "Angle.h"
#include "Point.h"

class DataNode;
class DataWriter;



// Class defining an AI "personality": what actions it takes, and how skilled
// and aggressive it is in combat. This also includes some more specialized
// behaviors, like plundering ships or launching surveillance drones, that are
// used to make some fleets noticeably different from others.
class Personality {
public :
	static constexpr int PACIFIST = (1 << 0);
	static constexpr int FORBEARING = (1 << 1);
	static constexpr int TIMID = (1 << 2);
	static constexpr int DISABLES = (1 << 3);
	static constexpr int PLUNDERS = (1 << 4);
	static constexpr int HEROIC = (1 << 5);
	static constexpr int STAYING = (1 << 6);
	static constexpr int ENTERING = (1 << 7);
	static constexpr int NEMESIS = (1 << 8);
	static constexpr int SURVEILLANCE = (1 << 9);
	static constexpr int UNINTERESTED = (1 << 10);
	static constexpr int WAITING = (1 << 11);
	static constexpr int DERELICT = (1 << 12);
	static constexpr int FLEEING = (1 << 13);
	static constexpr int ESCORT = (1 << 14);
	static constexpr int FRUGAL = (1 << 15);
	static constexpr int COWARD = (1 << 16);
	static constexpr int VINDICTIVE = (1 << 17);
	static constexpr int SWARMING = (1 << 18);
	static constexpr int UNCONSTRAINED = (1 << 19);
	static constexpr int MINING = (1 << 20);
	static constexpr int HARVESTS = (1 << 21);
	static constexpr int APPEASING = (1 << 22);
	static constexpr int MUTE = (1 << 23);
	static constexpr int OPPORTUNISTIC = (1 << 24);
	static constexpr int TARGET = (1 << 25);
	static constexpr int MARKED = (1 << 26);
	static constexpr int LAUNCHING = (1 << 27);


public:
	Personality() noexcept;
	
	void Load(const DataNode &node);
	void Save(DataWriter &out) const;
	
	// Who a ship decides to attack:
	bool IsPacifist() const;
	bool IsForbearing() const;
	bool IsTimid() const;
	bool IsHeroic() const;
	bool IsNemesis() const;
	
	// How they fight:
	bool IsFrugal() const;
	bool Disables() const;
	bool Plunders() const;
	bool IsVindictive() const;
	bool IsUnconstrained() const;
	bool IsCoward() const;
	bool IsAppeasing() const;
	bool IsOpportunistic() const;
	
	// Mission NPC states:
	bool IsStaying() const;
	bool IsEntering() const;
	bool IsWaiting() const;
	bool IsLaunching() const;
	bool IsFleeing() const;
	bool IsDerelict() const;
	bool IsUninterested() const;
	
	// Non-combat goals:
	bool IsSurveillance() const;
	bool IsMining() const;
	bool Harvests() const;
	bool IsSwarming() const;
	
	// Special flags:
	bool IsEscort() const;
	bool IsTarget() const;
	bool IsMarked() const;
	bool IsMute() const;
	
	// Current inaccuracy in this ship's targeting:
	const Point &Confusion() const;
	void UpdateConfusion(bool isFiring);
	
	// Personality to use for ships defending a planet from domination:
	static Personality Defender();
	
	
private:
	void Parse(const DataNode &node, int index, bool remove);
	
	
private:
	int flags;
	double confusionMultiplier;
	double aimMultiplier;
	Point confusion;
	Point confusionVelocity;

	friend class FleetEditor;
};



#endif
