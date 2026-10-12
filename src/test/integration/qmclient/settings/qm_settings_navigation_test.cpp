#include <engine/shared/config.h>

#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/SettingsNavigation.h>
#include <game/client/QmUi/cards/QmCardCatalog.h>
#include <game/client/components/menus.h>
#include <game/client/components/settings_runtime_cache.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>

TEST(QmSettingsNavigation, SidebarFollowsPlayerTasks)
{
	const std::vector<int> vExpected = {
		CMenus::SETTINGS_LANGUAGE, CMenus::SETTINGS_FRIENDS, CMenus::SETTINGS_TEE,
		CMenus::SETTINGS_APPEARANCE, CMenus::SETTINGS_ASSETS, CMenus::SETTINGS_SOUND,
		CMenus::SETTINGS_CONTROLS, CMenus::SETTINGS_HUD, CMenus::SETTINGS_QMCLIENT,
		CMenus::SETTINGS_SEARCH, CMenus::SETTINGS_CONTRIBUTORS};
	EXPECT_EQ(qm_settings_navigation::PageOrder(), vExpected);
	EXPECT_EQ(qm_settings_navigation::Views(CMenus::SETTINGS_APPEARANCE).size(), 2u);
	EXPECT_EQ(qm_settings_navigation::Views(CMenus::SETTINGS_CONTROLS).size(), 3u);
}

TEST(QmSettingsNavigation, LegacyPageAndSubtabKeepTheirSemanticDestination)
{
	const auto Hud = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_APPEARANCE, CMenus::APPEARANCE_TAB_HUD);
	EXPECT_EQ(Hud.m_Page, CMenus::SETTINGS_HUD);
	const auto Nameplate = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_APPEARANCE, CMenus::APPEARANCE_TAB_NAME_PLATE);
	EXPECT_EQ(Nameplate.m_Page, CMenus::SETTINGS_TEE);
	EXPECT_EQ(Nameplate.m_SubTab, 2);
	const auto Config = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_CONFIGS);
	EXPECT_EQ(Config.m_Page, CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(Config.m_SubTab, 4);
	const auto Workshop = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_TCLIENT, 0, 1);
	EXPECT_EQ(Workshop.m_Page, CMenus::SETTINGS_CONTROLS);
	EXPECT_EQ(Workshop.m_SubTab, 1);
	const auto Warlist = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_TCLIENT, 0, 2);
	EXPECT_EQ(Warlist.m_Page, CMenus::SETTINGS_FRIENDS);
	EXPECT_EQ(Warlist.m_SubTab, 1);
	const auto Features = qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_QMCLIENT, 0, 0, CMenus::QMCLIENT_SETTINGS_TAB_FUNCTION);
	EXPECT_EQ(Features.m_Page, CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(Features.m_SubTab, 0);
	const auto ModernVisual = qm_settings_navigation::ResolveCardTab("modern-visual-graphics");
	EXPECT_EQ(ModernVisual.m_Page, CMenus::SETTINGS_APPEARANCE);
	EXPECT_STREQ(ModernVisual.m_pViewId, "modern-visual-graphics");
	EXPECT_FALSE(qm_settings_navigation::ResolveLegacyPage(-1).IsValid());
	EXPECT_FALSE(qm_settings_navigation::ResolveLegacyPage(CMenus::SETTINGS_LENGTH).IsValid());
}

TEST(QmSettingsNavigation, CardsUseTheirFeatureCategoryAndAliasesUseTheirHost)
{
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:focus_mode", "visual").m_pViewId, "modern-hud");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:voice", "hud").m_pViewId, "modern-sound");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:bind_editor", "bind").m_pViewId, "modern-controls-overlay");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:input_overlay", "hud").m_pViewId, "modern-controls-overlay");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:friend_notify", "function").m_pViewId, "modern-friends-notifications");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:laser", "visual").m_pViewId, "modern-visual-graphics");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("qm:nameplate_text", "hud").m_pViewId, "modern-tee-effects");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("tclient:visual-font-cursor", "tclient").m_pViewId, "modern-visual-ui");
	EXPECT_STREQ(qm_settings_navigation::ResolveCard("deck:graphics-visual", "graphics").m_pViewId, "modern-visual-ui");
}

TEST(QmSettingsNavigation, MigrationPreservesUserColumnsAndRelativeOrder)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"qm:console", "visual", 1, 4},
		{"qm:tooltip", "visual", 1, 1},
		{"qm:focus_mode", "visual", 2, 2},
		{"qm:camera_view", "function", 2, 0},
	});
	ASSERT_TRUE(qm_settings_navigation::MigrateLayout(Model));
	EXPECT_EQ(Model.StableIdOrder("qm:", "modern-visual-ui", 1), (std::vector<std::string>{"qm:tooltip", "qm:console"}));
	EXPECT_EQ(Model.Entry(Model.FindByStableId("qm:focus_mode")).m_Column, 2);
	EXPECT_STREQ(Model.Entry(Model.FindByStableId("qm:focus_mode")).m_pDefaultTab, "modern-hud");
	EXPECT_EQ(Model.Entry(Model.FindByStableId("qm:camera_view")).m_Column, 2);
	EXPECT_STREQ(Model.Entry(Model.FindByStableId("qm:camera_view")).m_pDefaultTab, "modern-features-qm");
	const uint64_t Revision = Model.LayoutRevision();
	EXPECT_FALSE(qm_settings_navigation::MigrateLayout(Model));
	EXPECT_EQ(Model.LayoutRevision(), Revision);
}

TEST(QmSettingsNavigation, ExistingModernPlacementRemainsTheSearchLocateDestination)
{
	qm_card_order::CModel Model;
	Model.SetEntries(qm_card_registry::BuildDefaultEntries());
	Model.MoveToTab("deck:appearance-laser-enhanced", "modern-controls-workshop", 2, 0);
	ASSERT_TRUE(qm_settings_navigation::MigrateLayout(Model));
	const auto *pLaser = qm_card_registry::FindByStableId("qm:laser");
	ASSERT_NE(pLaser, nullptr);
	const auto Target = qm_card_registry::ResolveCardNavigationTarget(*pLaser, Model);
	EXPECT_STREQ(Target.m_pStableId, "deck:appearance-laser-enhanced");
	const auto Route = qm_settings_navigation::ResolveCard(Target.m_pStableId, Target.m_pTab);
	EXPECT_EQ(Route.m_Page, CMenus::SETTINGS_CONTROLS);
	EXPECT_EQ(Route.m_SubTab, 1);
	const auto vIds = qm_settings_navigation::CardIdsForView(Route.m_pViewId, Model, false);
	EXPECT_NE(std::find_if(vIds.begin(), vIds.end(), [](const char *pId) { return std::string(pId) == "deck:appearance-laser-enhanced"; }), vIds.end());
}

TEST(QmSettingsNavigation, EveryApplicableCatalogCardHasOneReachableDestination)
{
	qm_card_order::CModel Model;
	Model.SetEntries(qm_card_registry::BuildDefaultEntries());
	qm_settings_navigation::MigrateLayout(Model);
	for(const bool Sixup : {false, true})
	{
		std::vector<std::string> vVisible;
		for(const int Page : qm_settings_navigation::PageOrder())
			for(const auto &View : qm_settings_navigation::Views(Page))
				for(const char *pId : qm_settings_navigation::CardIdsForView(View.m_pId, Model, Sixup))
					vVisible.emplace_back(pId);
		for(const auto &Card : qm_card_registry::Defaults())
		{
			if(!qm_card_catalog::HasCardModule(Card.m_pStableId))
				continue;
			SCOPED_TRACE(Card.m_pStableId);
			const auto Route = qm_settings_navigation::ResolveCard(Card.m_pStableId, Model.Entry(Model.FindByStableId(Card.m_pStableId)).m_pDefaultTab);
			ASSERT_TRUE(Route.IsValid());
			if(Route.m_Page == CMenus::SETTINGS_CONTRIBUTORS)
				continue;
			const std::string Id = Card.m_pStableId;
			const bool Supported = Sixup ? Id != "deck:tee-skin-list" && Id != "deck:tee-skin-options" && Id != "deck:tee-skin-queue" : Id != "deck:tee7-editor";
			EXPECT_EQ(std::count(vVisible.begin(), vVisible.end(), Id), Supported ? 1 : 0);
		}
	}
}

TEST(QmSettingsNavigation, CompleteMigratedLayoutFitsExistingStorageAndSurvivesReload)
{
	qm_card_order::CModel Model;
	Model.SetEntries(qm_card_registry::BuildDefaultEntries());
	qm_settings_navigation::MigrateLayout(Model);
	std::array<char, sizeof(g_Config.m_QmGlobalCardOrder)> aOrder{};
	ASSERT_TRUE(Model.Serialize(aOrder.data(), static_cast<int>(aOrder.size())));
	qm_card_order::CModel Reloaded;
	ASSERT_TRUE(Reloaded.LoadMerged(aOrder.data(), qm_card_registry::BuildDefaultEntries()));
	EXPECT_FALSE(qm_settings_navigation::MigrateLayout(Reloaded));
	ASSERT_EQ(Reloaded.Count(), Model.Count());
	for(int Index = 0; Index < Model.Count(); ++Index)
	{
		const auto &Entry = Model.Entry(Index);
		const auto &Restored = Reloaded.Entry(Reloaded.FindByStableId(Entry.m_pStableId));
		SCOPED_TRACE(Entry.m_pStableId);
		EXPECT_STREQ(Restored.m_pDefaultTab, Entry.m_pDefaultTab);
		EXPECT_EQ(Restored.m_Column, Entry.m_Column);
		EXPECT_EQ(Restored.m_OrderInColumn, Entry.m_OrderInColumn);
	}
}

TEST(QmSettingsNavigation, CardProtocolAvailabilityUsesStableIdentityAfterLayoutMigration)
{
	for(const char *pId : {"deck:tee-skin-list", "deck:tee-skin-options", "deck:tee-skin-queue"})
	{
		SCOPED_TRACE(pId);
		EXPECT_TRUE(qm_settings_navigation::CardAvailable(pId, false));
		EXPECT_FALSE(qm_settings_navigation::CardAvailable(pId, true));
	}
	EXPECT_FALSE(qm_settings_navigation::CardAvailable("deck:tee7-editor", false));
	EXPECT_TRUE(qm_settings_navigation::CardAvailable("deck:tee7-editor", true));
	EXPECT_TRUE(qm_settings_navigation::CardAvailable("qm:console", false));
	EXPECT_TRUE(qm_settings_navigation::CardAvailable("qm:console", true));
}

TEST(QmSettingsNavigation, CacheContextUsesEachModernSubtabAndClampsToItsPage)
{
	const SSettingsNavigationTabs Tabs{2, 3, 1, 2, 4, 1};
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_FRIENDS, Tabs), 2);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_TEE, Tabs), 3);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_APPEARANCE, Tabs), 1);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_CONTROLS, Tabs), 2);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_QMCLIENT, Tabs), 4);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_CONTRIBUTORS, Tabs), 1);
	for(const int Page : {CMenus::SETTINGS_LANGUAGE, CMenus::SETTINGS_SOUND, CMenus::SETTINGS_HUD})
		EXPECT_EQ(SettingsNavigationSubtab(Page, Tabs), 0);
	for(const int Page : std::array<int, 3>{CMenus::SETTINGS_ASSETS, CMenus::SETTINGS_SEARCH, -1})
		EXPECT_EQ(SettingsNavigationSubtab(Page, Tabs), -1);
	const SSettingsNavigationTabs InvalidTabs{-3, 20, -2, 80, 90, 40};
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_FRIENDS, InvalidTabs), 0);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_TEE, InvalidTabs), 3);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_APPEARANCE, InvalidTabs), 0);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_CONTROLS, InvalidTabs), 2);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_QMCLIENT, InvalidTabs), 4);
	EXPECT_EQ(SettingsNavigationSubtab(CMenus::SETTINGS_CONTRIBUTORS, InvalidTabs), 1);
}

TEST(QmSettingsNavigation, WarmupInterpretsReusedPageNumbersOnlyBeforeNavigationMigration)
{
	const SSettingsNavigationTabs Tabs{2, 3, 1, 2, 4, 1};
	const auto LegacyHud = ResolveSettingsWarmupView(CMenus::SETTINGS_APPEARANCE, 0, Tabs, CMenus::APPEARANCE_TAB_HUD);
	EXPECT_EQ(LegacyHud.m_Page, CMenus::SETTINGS_HUD);
	EXPECT_EQ(LegacyHud.m_Tab, 0);
	const auto ModernVisual = ResolveSettingsWarmupView(CMenus::SETTINGS_APPEARANCE, 1, Tabs, CMenus::APPEARANCE_TAB_HUD);
	EXPECT_EQ(ModernVisual.m_Page, CMenus::SETTINGS_APPEARANCE);
	EXPECT_EQ(ModernVisual.m_Tab, 1);
	const auto LegacyFeatures = ResolveSettingsWarmupView(CMenus::SETTINGS_QMCLIENT, 0, Tabs, 0, 0, CMenus::QMCLIENT_SETTINGS_TAB_FUNCTION);
	EXPECT_EQ(LegacyFeatures.m_Page, CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(LegacyFeatures.m_Tab, 0);
	const auto ModernFeatures = ResolveSettingsWarmupView(CMenus::SETTINGS_QMCLIENT, 1, Tabs);
	EXPECT_EQ(ModernFeatures.m_Tab, 4);
	const auto HiddenConfigs = ResolveSettingsWarmupView(CMenus::SETTINGS_CONFIGS, 1, Tabs);
	EXPECT_EQ(HiddenConfigs.m_Page, CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(HiddenConfigs.m_Tab, 4);
	const auto HiddenWorkshop = ResolveSettingsWarmupView(CMenus::SETTINGS_TCLIENT, 1, Tabs, 0, 1);
	EXPECT_EQ(HiddenWorkshop.m_Page, CMenus::SETTINGS_CONTROLS);
	EXPECT_EQ(HiddenWorkshop.m_Tab, 1);
	const auto InvalidPage = ResolveSettingsWarmupView(-1, 1, Tabs);
	EXPECT_EQ(InvalidPage.m_Page, CMenus::SETTINGS_LANGUAGE);
	EXPECT_EQ(InvalidPage.m_Tab, 0);
}

TEST(QmSettingsNavigation, WarmupIncludesEveryModernViewOnceAndPrioritizesCurrentView)
{
	const auto vViews = BuildSettingsWarmupViews(CMenus::SETTINGS_CONTROLS, 2);
	ASSERT_FALSE(vViews.empty());
	EXPECT_EQ(vViews.front().m_Page, CMenus::SETTINGS_CONTROLS);
	EXPECT_EQ(vViews.front().m_Tab, 2);
	std::set<std::pair<int, int>> Seen;
	for(const auto &View : vViews)
	{
		SCOPED_TRACE(View.m_Page);
		EXPECT_TRUE(SettingsPageVisibleInRightTabBar(View.m_Page));
		EXPECT_TRUE(Seen.emplace(View.m_Page, View.m_Tab).second);
	}
	for(const int Page : qm_settings_navigation::PageOrder())
	{
		const int NumTabs = Page == CMenus::SETTINGS_CONTRIBUTORS ? CMenus::CREDITS_SETTINGS_TAB_NUM : static_cast<int>(qm_settings_navigation::Views(Page).size());
		if(NumTabs == 0)
			EXPECT_EQ(Seen.count({Page, -1}), 1u);
		else
			for(int Tab = 0; Tab < NumTabs; ++Tab)
				EXPECT_EQ(Seen.count({Page, Tab}), 1u);
	}
	const auto vLegacyPreferred = BuildSettingsWarmupViews(CMenus::SETTINGS_PROFILES, -1);
	ASSERT_FALSE(vLegacyPreferred.empty());
	EXPECT_EQ(vLegacyPreferred.front().m_Page, CMenus::SETTINGS_TEE);
	EXPECT_EQ(vLegacyPreferred.front().m_Tab, 3);
}

TEST(QmSettingsNavigation, WarmupPriorityPreservesOtherViewsOrderAndCacheKeysSeparateSubtabs)
{
	std::vector<std::pair<int, int>> vDefaultOrder;
	for(const auto &View : BuildSettingsWarmupViews(CMenus::SETTINGS_LANGUAGE, 0))
		if(View.m_Page != CMenus::SETTINGS_APPEARANCE || View.m_Tab != 1)
			vDefaultOrder.emplace_back(View.m_Page, View.m_Tab);
	const auto vPreferred = BuildSettingsWarmupViews(CMenus::SETTINGS_APPEARANCE, 1);
	ASSERT_FALSE(vPreferred.empty());
	EXPECT_EQ(vPreferred.front().m_Page, CMenus::SETTINGS_APPEARANCE);
	EXPECT_EQ(vPreferred.front().m_Tab, 1);
	std::vector<std::pair<int, int>> vRemaining;
	for(size_t Index = 1; Index < vPreferred.size(); ++Index)
		vRemaining.emplace_back(vPreferred[Index].m_Page, vPreferred[Index].m_Tab);
	EXPECT_EQ(vRemaining, vDefaultOrder);
	EXPECT_NE(SettingsTextCacheKey(CMenus::SETTINGS_CONTROLS, 1, "shared-setting"), SettingsTextCacheKey(CMenus::SETTINGS_CONTROLS, 2, "shared-setting"));
	EXPECT_NE(SettingsPageCacheKey(CMenus::SETTINGS_FRIENDS, 0), SettingsPageCacheKey(CMenus::SETTINGS_HUD, 0));
}
