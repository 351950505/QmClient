#include "SettingsNavigation.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>
#include <engine/shared/localization.h>

#include <game/client/QmUi/QmScroll.h>
#include <game/client/QmUi/SettingsCardDeck.h>
#include <game/client/QmUi/UiNavigation.h>
#include <game/client/QmUi/cards/QmCardCatalog.h>
#include <game/client/QmUi/cards/QmCardCatalogTClientInternal.h>
#include <game/client/QmUi/cards/QmCardMeasureRevision.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

void CMenus::RenderModernSettingsPage(CUIRect View, const bool PrewarmOnly)
{
	std::optional<qm_tclient_cards::CUiRenderOnlyGuard> ReadOnlyGuard;
	if(PrewarmOnly && !Ui()->MeasurementOnly())
		ReadOnlyGuard.emplace(Ui());
	const bool MeasurementOnly = Ui()->MeasurementOnly();
	const bool ReadOnly = Ui()->RenderOnly();
	const int Page = g_Config.m_UiSettingsPage;
	const auto &vViews = qm_settings_navigation::Views(Page);
	if(vViews.empty())
		return;
	int *pSelected = nullptr;
	switch(Page)
	{
	case SETTINGS_FRIENDS: pSelected = &m_ModernFriendsTab; break;
	case SETTINGS_TEE: pSelected = &m_ModernTeeTab; break;
	case SETTINGS_APPEARANCE: pSelected = &m_ModernVisualTab; break;
	case SETTINGS_CONTROLS: pSelected = &m_ModernControlsTab; break;
	case SETTINGS_QMCLIENT: pSelected = &m_ModernFeaturesTab; break;
	}
	int Selected = pSelected != nullptr ? std::clamp(*pSelected, 0, static_cast<int>(vViews.size()) - 1) : 0;
	if(vViews.size() > 1)
	{
		const auto Metrics = ResolveSettingsContentMetrics(View.w);
		const auto Layout = ResolveSettingsSubTabLayout(View, Metrics.m_UiScale);
		View = Layout.m_ContentRect;
		CUIRect TabBar = Layout.m_TabBarRect;
		TabBar.w = std::min(TabBar.w, static_cast<float>(vViews.size()) * 148.0f * Metrics.m_UiScale);
		struct SSubTabUi
		{
			std::vector<CUIRect> m_vSlots;
			std::vector<CButtonContainer> m_vButtons;
		};
		static std::map<int, SSubTabUi> s_TabUi;
		auto &TabUi = s_TabUi[Page];
		auto &vSlots = TabUi.m_vSlots;
		auto &vButtons = TabUi.m_vButtons;
		vSlots.resize(vViews.size());
		vButtons.resize(vViews.size());
		for(size_t Index = 0; Index < vSlots.size(); ++Index)
			vSlots[Index] = {TabBar.x + TabBar.w * Index / vSlots.size(), TabBar.y, TabBar.w / vSlots.size(), TabBar.h};
		const uint64_t Scope = MakeUiScopeHash("modern-settings-sub-tabs") ^ static_cast<uint64_t>(Page);
		ui_widget::CapsuleTabBarChrome(TabBarUiContext(), Scope, vSlots.data(), vSlots.size(), Selected, SettingsCapsuleTabBarStyle());
		for(size_t Index = 0; Index < vSlots.size(); ++Index)
		{
			if(DoButton_MenuTab(&vButtons[Index], Localize(vViews[Index].m_pLabel), Selected == static_cast<int>(Index), &vSlots[Index], IGraphics::CORNER_ALL, nullptr, nullptr, nullptr, nullptr, 4.0f, nullptr, nullptr, -1.0f, true))
				Selected = static_cast<int>(Index);
		}
	}
	if(!MeasurementOnly && !m_MenuTextPlanCollecting)
	{
		if(pSelected != nullptr)
			*pSelected = Selected;
		// 子页和身份选择共用一个显示周期，避免壳层与卡片层每帧交替重置。
		const uint64_t DisplayKey = (static_cast<uint64_t>(Page) << 32) | (static_cast<uint64_t>(Selected + 1) << 1) | (Page == SETTINGS_TEE && m_Dummy ? 1u : 0u);
		if(m_SettingsCardDeckDisplayState.EnterView(DisplayKey))
		{
			m_SettingsCardDeck.BeginDisplayCycle(++m_SettingsCardDeckDisplayCycle, true);
			if(Page == SETTINGS_TEE)
			{
				CommitSettingsTeeSkinEdits();
				m_TeeEntranceStartTime = time_get();
			}
		}
	}
	// 点击子页当帧，卡片文字须使用新子页的缓存身份。
	m_SettingsTextContextTab = Selected;
	m_SettingsTextContextSubtab = Selected;
	if(Page == SETTINGS_LANGUAGE)
	{
		if(!ReadOnly)
			m_SettingsCardFocusStableId.clear();
		RenderLanguageSettings(View);
		return;
	}
	if(Page == SETTINGS_FRIENDS && Selected == 0)
	{
		if(!MeasurementOnly)
			RenderServerbrowserFriends(View);
		return;
	}
	if(Page == SETTINGS_TEE && Selected == 1)
	{
		RenderSettingsPlayer(View);
		return;
	}
	RenderModernSettingsCards(View, vViews[Selected].m_pId, PrewarmOnly);
}

void CMenus::RenderModernSettingsCards(CUIRect View, const char *pViewId, const bool PrewarmOnly)
{
	EnsureSettingsBindCache();
	std::optional<qm_tclient_cards::CUiRenderOnlyGuard> ReadOnlyGuard;
	if(PrewarmOnly && !Ui()->MeasurementOnly())
		ReadOnlyGuard.emplace(Ui());
	const bool MeasurementOnly = Ui()->MeasurementOnly();
	const bool ReadOnly = Ui()->RenderOnly();
	static std::map<std::string, CScrollRegion> s_ScrollRegions;
	CScrollRegion *pScroll = MeasurementOnly ? nullptr : &s_ScrollRegions[pViewId];
	auto &Model = SettingsCardOrderModelForRenderPass();
	qm_card_catalog::SQmCardBuildContext Ctx;
	Ctx.m_pMenus = this;
	Ctx.m_ReadOnly = ReadOnly;
	Ctx.m_Metrics = ResolveSettingsContentMetrics(View.w);
	Ctx.m_Page = SettingsPageLayout(View, Ctx.m_Metrics.m_UiScale);
	Ctx.m_LabelWidth = ResolveSettingsCardLabelWidth(Ctx.m_Page.m_TwoColumns ? Ctx.m_Page.m_aColumns[0].w : Ctx.m_Page.m_ContentViewport.w, Ctx.m_Metrics);
	Ctx.m_UiContext = SettingsUiContext(pViewId, Ctx.m_Metrics.m_UiScale);
	Ctx.m_pScrollRegion = pScroll;
	if(MeasurementOnly)
	{
		Ctx.m_UiContext.m_pAnim = nullptr;
		Ctx.m_UiContext.m_pTree = nullptr;
	}
	const auto Style = QmSettingsCardStyle(Ctx.m_Metrics.m_UiScale);
	Ctx.m_Padding = Style.m_Padding;
	Ctx.m_CornerRadius = Style.m_CornerRadius;
	// 卡片闭包跨帧保存，布局状态须拥有稳定地址。
	static qm_card_catalog::SQmFunctionCardLayoutState s_FunctionLayout;
	s_FunctionLayout = qm_card_catalog::ResolveFunctionCardLayoutState(qm_card_catalog::QmCardRenderHook::FavoriteMapCount(this));
	Ctx.m_pFunctionLayout = &s_FunctionLayout;
	struct SCardListCache
	{
		const qm_card_order::CModel *m_pModel = nullptr;
		uint64_t m_LayoutRevision = 0;
		bool m_Sixup = false;
		std::vector<const char *> m_vIds;
	};
	static std::map<std::string, SCardListCache> s_CardLists;
	auto &CardList = s_CardLists[pViewId];
	if(CardList.m_pModel != &Model || CardList.m_LayoutRevision != Model.LayoutRevision() || CardList.m_Sixup != Client()->IsSixup())
	{
		CardList.m_vIds = qm_settings_navigation::CardIdsForView(pViewId, Model, Client()->IsSixup());
		CardList.m_pModel = &Model;
		CardList.m_LayoutRevision = Model.LayoutRevision();
		CardList.m_Sixup = Client()->IsSixup();
	}
	const auto &vIds = CardList.m_vIds;
	uint64_t ContentRevision = qm_card_catalog::QmCardRenderHook::PrepareStandardCards(Ctx, vIds);
	ContentRevision = ContentRevision * 1099511628211ULL ^ qm_card_catalog::QmCardRenderHook::PrepareTClientCards(Ctx, vIds);
	ContentRevision = ContentRevision * 1099511628211ULL ^ qm_card_catalog::MeasureModuleCardsRevision(s_FunctionLayout);
	ContentRevision = ContentRevision * 1099511628211ULL ^ TranslationTestLayoutRevision();
	ContentRevision = ContentRevision * 1099511628211ULL ^ qm_card_catalog::MeasureContentRevision(g_Localization.Languages().size(), GameClient()->m_MenuBackground.GetThemes().size());
	if(std::any_of(vIds.begin(), vIds.end(), [](const char *pId) { return str_startswith(pId, "deck:controls-") != nullptr; }))
		ContentRevision = ContentRevision * 1099511628211ULL ^ qm_card_catalog::QmCardRenderHook::PrepareControlsCards(this, View.w, ReadOnly, pScroll);
	ContentRevision = ContentRevision * 1099511628211ULL ^ Model.LayoutRevision();
	ContentRevision = ContentRevision * 1099511628211ULL ^ str_quickhash(pViewId);
	ContentRevision = ContentRevision * 1099511628211ULL ^ (Client()->IsSixup() ? 1u : 0u);
	ContentRevision = ContentRevision * 1099511628211ULL ^ (ReadOnly ? 1u : 0u);
	ContentRevision = ContentRevision * 1099511628211ULL ^ static_cast<uint64_t>(View.h * 100.0f);
	const uint64_t Revision = ResolveSettingsCardDefinitionsRevision(m_SettingsCardDeckDisplayCycle, m_MenuTextPoolGeneration, View.w, ContentRevision);
	const auto BuildDefinitions = [Ctx, &vIds](std::vector<SSettingsCardDefinition> &vCards) {
		qm_card_catalog::BuildCards(Ctx, vIds, vCards);
	};
	if(!ReadOnly && !m_SettingsCardFocusStableId.empty())
	{
		m_SettingsCardDeck.RequestReveal(m_SettingsCardFocusStableId.c_str());
		m_SettingsCardFocusStableId.clear();
	}
	const auto ScrollParams = QmScrollRegionParamsFromPolicy(QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_OUTER}, Ctx.m_Metrics.m_UiScale, 0.0f));
	SSettingsCardDeckInput InputState;
	InputState.m_AllowHeaderDrag = !ReadOnly;
	InputState.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
	InputState.m_pScrollParams = MeasurementOnly ? nullptr : &ScrollParams;
	if(!ReadOnly)
	{
		InputState.m_MouseX = Ui()->MouseX();
		InputState.m_MouseY = Ui()->MouseY();
		InputState.m_MousePressed = Ui()->MouseButtonClicked(0);
		InputState.m_MouseDown = Ui()->MouseButton(0);
		InputState.m_MouseReleased = !InputState.m_MouseDown && Ui()->LastMouseButton(0);
		InputState.m_CtrlPressed = Input()->ModifierIsPressed();
	}
	auto VisualOptions = SettingsCardDeckVisualOptions();
	if(str_comp(pViewId, "modern-tee") == 0)
		VisualOptions.m_LeadingFullWidthCards = 2;
	const uint64_t MeasurementEnvironmentRevision = ResolveSettingsCardDefinitionsRevision(m_SettingsCardDeckDisplayCycle, m_MenuTextPoolGeneration, View.w, 0);
	const auto Result = SettingsCardDeckForRenderPass().RenderCached(Ctx.m_UiContext, Ctx.m_Page, pViewId, Revision, BuildDefinitions, Model, pScroll, InputState, SettingsCardMotionSpec(), VisualOptions, MeasurementEnvironmentRevision);
	if(!ReadOnly && Result.m_OrderChanged)
		SaveSettingsCardOrderModel();
}
