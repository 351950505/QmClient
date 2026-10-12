#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_MENUS_TCLIENT_INTERNAL_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_MENUS_TCLIENT_INTERNAL_H

#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/cards/QmCardCatalogTClientInternal.h>

namespace qm_tclient_menu
{
	inline int CanonicalizePersistedTClientTab(int Tab)
	{
		if(Tab < 0 || Tab >= qm_tclient_cards::NUMBER_OF_TCLIENT_TABS)
			return qm_tclient_cards::TCLIENT_TAB_SETTINGS;
		return Tab;
	}

	inline int CanonicalizePersistedQmClientTab(int Tab)
	{
		// 贡献者子页签并入顶层贡献者页后枚举值只作占位；持久化里的旧值回落到首个子页签。
		if(Tab == CMenus::QMCLIENT_SETTINGS_TAB_CONTRIBUTORS)
			return CMenus::QMCLIENT_SETTINGS_TAB_VISUAL;
		if(Tab < 0 || Tab >= CMenus::NUMBER_OF_QMCLIENT_SETTINGS_TABS)
			return CMenus::QMCLIENT_SETTINGS_TAB_VISUAL;
		return Tab;
	}

	inline CUIRect TClientSettingsContentView(CUIRect MainView, CUIRect *pTabBar = nullptr)
	{
		const SSettingsContentMetrics Metrics = ResolveSettingsContentMetrics(MainView.w);
		const SSettingsSubTabLayoutFrame SubTabs = ResolveSettingsSubTabLayout(MainView, Metrics.m_UiScale);
		if(pTabBar != nullptr)
			*pTabBar = SubTabs.m_TabBarRect;
		return SubTabs.m_ContentRect;
	}

	// 设置页推进开发者 Tee 预览时间；绘制和预热沿用同一份进程内状态。
	void AdvancePreviewTime(float FrameTime);
}

#endif
