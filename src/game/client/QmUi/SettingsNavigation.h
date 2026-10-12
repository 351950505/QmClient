#ifndef GAME_CLIENT_QMUI_SETTINGSNAVIGATION_H
#define GAME_CLIENT_QMUI_SETTINGSNAVIGATION_H

#include <game/client/QmUi/QmCardOrderModel.h>

#include <vector>

namespace qm_settings_navigation
{
	struct SView
	{
		const char *m_pId;
		const char *m_pLabel;
	};

	struct SRoute
	{
		int m_Page = -1;
		int m_SubTab = 0;
		const char *m_pViewId = nullptr;
		bool IsValid() const { return m_Page >= 0; }
	};

	const std::vector<int> &PageOrder();
	const char *PageLabel(int Page);
	const std::vector<SView> &Views(int Page);
	SRoute ResolveLegacyPage(int Page, int AppearanceTab = 0, int TClientTab = 0, int QmTab = 0);
	SRoute ResolveCardTab(const char *pTab);
	SRoute ResolveCard(const char *pStableId, const char *pSavedTab);
	// 旧位置映射到新分类，保留各来源组内的手动排序和列选择；新位置不重复迁移。
	bool MigrateLayout(qm_card_order::CModel &Model);
	bool CardAvailable(const char *pStableId, bool Sixup);
	std::vector<const char *> CardIdsForView(const char *pViewId, const qm_card_order::CModel &Model, bool Sixup);
}

#endif
