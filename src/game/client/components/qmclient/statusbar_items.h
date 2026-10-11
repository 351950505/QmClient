#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_STATUSBAR_ITEMS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_STATUSBAR_ITEMS_H

#include <base/str.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

enum
{
	STATUSBAR_MAX_SIZE = 128,
	STATUSBAR_TYPE_LETTERS = 4
};

class CStatusItem
{
public:
	std::function<void()> m_RenderItem;
	std::function<float()> m_GetWidth;
	char m_aName[32];
	char m_aDisplayName[32];
	char m_aDesc[128];
	char m_aLetters[STATUSBAR_TYPE_LETTERS] = {};
	bool m_ShowLabel = true;
	CStatusItem(std::function<void()> Render, std::function<float()> Width, const char *pLetters, const char *pName, const char *pDisplayName, const char *pDesc, bool ShowLabel = true)
	{
		m_RenderItem = std::move(Render);
		m_GetWidth = std::move(Width);
		str_copy(m_aLetters, pLetters);
		str_copy(m_aName, pName);
		str_copy(m_aDisplayName, pDisplayName[0] != '\0' ? pDisplayName : pName);
		str_copy(m_aDesc, pDesc);
		m_ShowLabel = ShowLabel;
	}
};

inline std::vector<CStatusItem *> QmParseStatusBarScheme(const char *pScheme, const std::vector<CStatusItem *> &vpTypes)
{
	std::vector<CStatusItem *> vpItems;
	for(int i = 0; pScheme[i] != '\0' && vpItems.size() < STATUSBAR_MAX_SIZE; ++i)
	{
		for(CStatusItem *pType : vpTypes)
		{
			if(std::find(std::begin(pType->m_aLetters), std::end(pType->m_aLetters), pScheme[i]) != std::end(pType->m_aLetters))
			{
				vpItems.push_back(pType);
				break;
			}
		}
	}
	return vpItems;
}

#endif
