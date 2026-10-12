#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MENUS_QMCLIENT_INTERNAL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MENUS_QMCLIENT_INTERNAL_H

#include <game/client/QmUi/cards/QmCardCatalog.h>

namespace qm_settings_menu
{
	// 原页和搜索页从卡片模块取得相同的编辑状态与布局版本。
	inline qm_card_catalog::SQmFunctionCardLayoutState ResolveFunctionCardLayoutState(size_t FavoriteMapCount)
	{
		qm_card_catalog::SQmFunctionCardLayoutState State;
		State.m_BlockWordsRevision = qm_card_catalog::BlockWordsLayoutRevision();
		State.m_FavoriteMapsRevision = qm_card_catalog::FavoriteMapsLayoutRevision(FavoriteMapCount);
		qm_card_catalog::FillKeywordReplyLayoutState(State);
		return State;
	}

}

#endif
