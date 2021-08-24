/* SpriteSet.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SpriteSet.h"

#include "Set.h"
#include "Sprite.h"

#include <map>

using namespace std;

namespace {
	Set<Sprite> sprites;
}



const Sprite *SpriteSet::Get(const string &name)
{
	return Modify(name);
}



const Set<Sprite> &SpriteSet::GetSprites()
{
	return sprites;
}



set<string> SpriteSet::CheckReferences()
{
	auto unloaded = set<string>{};
	for(const auto &pair : sprites)
	{
		const Sprite &sprite = pair.second;
		if(sprite.Height() == 0 && sprite.Width() == 0)
			unloaded.insert(pair.first);
	}
	return unloaded;
}



Sprite *SpriteSet::Modify(const string &name)
{
	auto it = sprites.Find(name);
	if(!it)
	{
		it = sprites.Get(name);
		*it = Sprite(name);
	}
	return it;
}
