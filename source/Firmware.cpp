/* Firmware.cpp
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

#include "Firmware.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DefenseDaemon.h"
#include "EscortRole.h"
#include "GameData.h"
#include "Government.h"
#include "MissileGuidance.h"
#include "ShipManeuver.h"
#include "Weapon.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

using namespace std;

namespace {
	// Whether a government is, or is a variant of, the one named.
	bool Matches(const Government *government, const string &name)
	{
		if(!government)
			return false;
		const string &own = government->TrueName();
		return own == name || !own.compare(0, name.size() + 2, name + " (");
	}
}



void Firmware::Load(const DataNode &node)
{
	if(node.Size() >= 2)
		name = node.Token(1);
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		if(key == "guidance" || key == "daemon" || key == "role")
			kind = key;
		else if(key == "maneuver" && child.Size() >= 2)
		{
			kind = key;
			replaces = child.Token(1);
		}
		else if(key == "author" && child.Size() >= 2)
			author = child.Token(1);
		else if(key == "flown by")
			for(int i = 1; i < child.Size(); ++i)
				flownBy.push_back(child.Token(i));
		else if(key == "price" && child.Size() >= 2)
			price = static_cast<int64_t>(child.Value(1));
		else if(key == "description" && child.Size() >= 2)
			description += (description.empty() ? "" : "\n") + child.Token(1);
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



const string &Firmware::Name() const
{
	return name;
}



bool Firmware::IsGuidance() const
{
	return kind == "guidance";
}



bool Firmware::IsDaemon() const
{
	return kind == "daemon";
}



const string &Firmware::Kind() const
{
	return kind;
}



const string &Firmware::Replaces() const
{
	return replaces;
}



const string &Firmware::Author() const
{
	return author;
}



int64_t Firmware::Price() const
{
	return price;
}



const string &Firmware::Description() const
{
	return description;
}



bool Firmware::IsFlownBy(const Government *government) const
{
	return any_of(flownBy.begin(), flownBy.end(), [government](const string &name)
		{ return Matches(government, name); });
}



bool Firmware::IsSoldBy(const Government *government) const
{
	return Matches(government, author) || IsFlownBy(government);
}



const Firmware *Firmware::GuidanceFor(const Government *government, const Weapon &weapon)
{
	if(!government)
		return nullptr;
	// Each government flies the first program (by name) that names it,
	// and that its weapon's hardware can run.
	static map<pair<const Government *, const Weapon *>, const Firmware *> cache;
	auto it = cache.find({government, &weapon});
	if(it != cache.end())
		return it->second;
	const Firmware *found = nullptr;
	for(const auto &[name, firmware] : GameData::Firmwares())
	{
		if(!firmware.IsGuidance() || !firmware.IsFlownBy(government))
			continue;
		const MissileGuidance *guidance = GameData::Guidance().Find(name);
		if(guidance && guidance->IsValid() && guidance->MissingCapability(weapon).empty())
		{
			found = &firmware;
			break;
		}
	}
	cache[{government, &weapon}] = found;
	return found;
}



const Firmware *Firmware::DaemonFor(const Government *government)
{
	if(!government)
		return nullptr;
	static map<const Government *, const Firmware *> cache;
	auto it = cache.find(government);
	if(it != cache.end())
		return it->second;
	const Firmware *found = nullptr;
	for(const auto &[name, firmware] : GameData::Firmwares())
		if(firmware.IsDaemon() && firmware.IsFlownBy(government))
		{
			const DefenseDaemon *daemon = GameData::Daemons().Find(name);
			if(daemon && daemon->IsValid())
			{
				found = &firmware;
				break;
			}
		}
	cache[government] = found;
	return found;
}



string Firmware::KindOf(const string &text)
{
	istringstream in(text);
	DataFile file(in);
	for(const DataNode &node : file)
	{
		const string &key = node.Token(0);
		return (key == "guidance" || key == "maneuver" || key == "daemon" || key == "role") && node.Size() >= 2
			? key : string();
	}
	return "";
}



string Firmware::NameOf(const string &text)
{
	istringstream in(text);
	DataFile file(in);
	for(const DataNode &node : file)
		return node.Size() >= 2 ? node.Token(1) : string();
	return "";
}



string Firmware::Renamed(const string &text, const string &name)
{
	// The node is the first line that starts (unindented) with its kind;
	// comments may come before it.
	const string kind = KindOf(text);
	if(kind.empty())
		return text;
	size_t start = 0;
	while(start < text.size())
	{
		const size_t end = text.find('\n', start);
		if(!text.compare(start, kind.size() + 1, kind + " "))
			return text.substr(0, start) + kind + " \"" + name + "\""
				+ (end == string::npos ? string() : text.substr(end));
		if(end == string::npos)
			break;
		start = end + 1;
	}
	return text;
}



bool Firmware::IsGameScript(const string &kind, const string &name)
{
	if(kind == "guidance")
		return GameData::Guidance().Has(name);
	if(kind == "maneuver")
		return GameData::Maneuvers().Has(name);
	if(kind == "daemon")
		return GameData::Daemons().Has(name);
	if(kind == "role")
		return GameData::Roles().Has(name);
	return false;
}
