/* CodeStyle.cpp
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

#include "CodeStyle.h"

#include "DataNode.h"
#include "GameData.h"
#include "Government.h"

#include <cctype>

using namespace std;



void CodeStyle::Load(const DataNode &node)
{
	if(node.Size() >= 2)
		name = node.Token(1);
	governments.clear();
	header.clear();
	colors.clear();
	cipher.clear();
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		if(key == "government")
			for(int i = 1; i < child.Size(); ++i)
				governments.push_back(child.Token(i));
		else if(key == "header" && child.Size() >= 2)
			header.push_back(child.Token(1));
		else if(key == "color" && child.Size() >= 5)
			colors[child.Token(1)] = Color(child.Value(2), child.Value(3), child.Value(4), 1.);
		else if(key == "cipher" && child.Size() >= 2 && child.Token(1).size() == 26)
			cipher = child.Token(1);
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



const CodeStyle *CodeStyle::ForGovernment(const Government *government)
{
	if(!government)
		return nullptr;
	const string &name = government->TrueName();
	for(const auto &it : GameData::CodeStyles())
		if(it.second.Covers(name))
			return &it.second;
	return nullptr;
}



const string &CodeStyle::Name() const
{
	return name;
}



bool CodeStyle::Covers(const string &government) const
{
	for(const string &it : governments)
		if(government == it || (government.size() > it.size() && government.compare(0, it.size(), it) == 0
				&& government[it.size()] == ' '))
			return true;
	return false;
}



const vector<string> &CodeStyle::Header() const
{
	return header;
}



const Color *CodeStyle::GetColor(const string &part) const
{
	auto it = colors.find(part);
	return it == colors.end() ? nullptr : &it->second;
}



string CodeStyle::Encode(const string &text) const
{
	if(cipher.empty())
		return text;
	string result = text;
	for(char &c : result)
	{
		const bool isUpper = isupper(static_cast<unsigned char>(c));
		const char lower = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		if(lower >= 'a' && lower <= 'z')
		{
			const char replacement = cipher[lower - 'a'];
			c = isUpper ? static_cast<char>(toupper(static_cast<unsigned char>(replacement))) : replacement;
		}
	}
	return result;
}



bool CodeStyle::HasCipher() const
{
	return !cipher.empty();
}
