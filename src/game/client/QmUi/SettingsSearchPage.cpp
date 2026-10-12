#include "QmAnimResolve.h"
#include "QmCardRegistry.h"
#include "QmScroll.h"
#include "SettingsPageLayout.h"
#include "SettingsSearchHighlight.h"
#include "SettingsSearchSession.h"
#include "UiContext.h"
#include "UiForms.h"
#include "UiSurface.h"
#include "cards/QmCardCatalogInternal.h"
#include "cards/QmCardMeasureRevision.h"

#include <base/system.h>

#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using SQmGlobalSearchCard = qm_card_registry::SCardSearchResult;

struct SQmGlobalSearchResults
{
	std::vector<SQmGlobalSearchCard> m_vAllVisibleCards;
};

namespace
{
	using TConfigAddresses = std::unordered_map<std::string, const void *>;

	TConfigAddresses SettingsSearchConfigAddresses(IConfigManager *pConfigManager)
	{
		TConfigAddresses Addresses;
		pConfigManager->PossibleConfigVariables("", CFGFLAG_CLIENT, [](const SConfigVariable *pVariable, void *pUser) {
			auto &Map = *static_cast<TConfigAddresses *>(pUser);
			const void *pAddress = nullptr;
			switch(pVariable->m_Type)
			{
			case SConfigVariable::VAR_INT: pAddress = static_cast<const SIntConfigVariable *>(pVariable)->m_pVariable; break;
			case SConfigVariable::VAR_COLOR: pAddress = static_cast<const SColorConfigVariable *>(pVariable)->m_pVariable; break;
			case SConfigVariable::VAR_STRING: pAddress = static_cast<const SStringConfigVariable *>(pVariable)->m_pStr; break;
			}
			Map.emplace(pVariable->m_pScriptName, pAddress); }, &Addresses);
		return Addresses;
	}

	void CollectGlobalSearchResults(const char *pSearch, bool Sixup, const qm_card_order::CModel &Model, SQmGlobalSearchResults &Out)
	{
		Out.m_vAllVisibleCards.clear();
		// 无搜索内容时结果为空：搜索页默认只显示搜索输入卡片，不展示全量卡片。
		if(pSearch == nullptr || pSearch[0] == '\0')
			return;
		std::vector<SQmGlobalSearchCard> vCards = qm_card_registry::SearchCards(pSearch, Model);
		Out.m_vAllVisibleCards.reserve(vCards.size());
		for(SQmGlobalSearchCard &Card : vCards)
		{
			const char *pTab = Card.m_Target.m_pTab;
			if(pTab != nullptr && str_comp(pTab, "global-search") == 0)
				continue;
			if(pTab != nullptr && ((Sixup && str_comp(pTab, "tee") == 0) || (!Sixup && str_comp(pTab, "tee7") == 0)))
				continue;
			Out.m_vAllVisibleCards.push_back(std::move(Card));
		}
	}
}

void CMenus::RenderSettingsGlobalSearch(CUIRect MainView, bool PrewarmOnly)
{
	RenderSettingsGlobalSearchContent(MainView, PrewarmOnly);
}

void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)
{
	const SSettingsContentMetrics Metrics = ResolveSettingsContentMetrics(MainView.w);
	const float UiScale = Metrics.m_UiScale;
	const float BodySize = Metrics.m_BodySize;
	const float SmallSize = Metrics.m_SmallSize;
	const float LineHeight = Metrics.m_LineHeight;
	const float LineSpacing = Metrics.m_LineSpacing;
	const SSettingsPageLayoutFrame Page = SettingsPageLayout(MainView, UiScale);
	const bool ReadOnly = PrewarmOnly || Ui()->RenderOnly();
	IUiContext SearchCtx = SettingsUiContext("settings_global_search", UiScale);
	if(ReadOnly)
	{
		SearchCtx.m_pAnim = nullptr;
		SearchCtx.m_pTree = nullptr;
	}
	static CScrollRegion s_GlobalSearchScrollRegion;
	static std::array<bool, qm_module::QmModuleCount> s_aGlobalSearchCollapsed = {};
	static std::array<CButtonContainer, qm_module::QmModuleCount> s_aGlobalSearchCollapseButtons;
	static std::unordered_map<std::string, bool> s_SearchCollapsed;
	auto ToggleSearchCollapsed = [](void *pUser, qm_module::EQmModuleId Id) {
		bool *pCollapsed = static_cast<bool *>(pUser);
		if(pCollapsed == nullptr)
			return;
		const int Index = std::clamp((int)Id, 0, (int)qm_module::QmModuleCount - 1);
		pCollapsed[Index] = !pCollapsed[Index];
	};

	CLineInputBuffered<128> &ModuleSearchInput = m_GlobalCardSearchInput;
	const char *pModuleSearch = ModuleSearchInput.GetString();
	struct SGlobalSearchCache
	{
		bool m_Valid = false;
		uint64_t m_ResultsRevision = 0;
		std::string m_Search;
		std::string m_Language;
		bool m_Sixup = false;
		uint64_t m_LayoutRevision = 0;
		SQmGlobalSearchResults m_Results;
		qm_card_order::CModel m_Model;
		std::unordered_map<std::string, std::shared_ptr<CSettingsSearchHighlight>> m_Highlights;
	};
	static SGlobalSearchCache s_GlobalSearchCache;
	static qm_card_order::CModel s_GlobalSearchPrewarmOrderModel;
	static CSettingsCardDeck s_GlobalSearchPrewarmDeck;
	static CSettingsCardDeck s_GlobalSearchLiveDeck;
	static std::string s_LastRevealRequest;
	static std::string s_LiveSearchQuery;
	static uint64_t s_LiveResultsRevision = 0;
	static qm_card_catalog::SQmCardBuildContext s_GlobalSearchCardBuild;
	static qm_card_catalog::SQmFunctionCardLayoutState s_GlobalSearchFunctionCardLayout;
	static std::unordered_map<std::string, CButtonContainer> s_GlobalSearchActionButtons;
	// 分类布局只用于解析搜索和导航；以下两个模型仅属于搜索页，不持久化。
	const qm_card_order::CModel &CardOrderModel = SettingsCardOrderModel();
	qm_card_order::CModel &DeckOrderModel = ReadOnly ? s_GlobalSearchPrewarmOrderModel : s_GlobalSearchCache.m_Model;
	CSettingsCardDeck &CardDeck = ReadOnly ? s_GlobalSearchPrewarmDeck : s_GlobalSearchLiveDeck;
	const char *pLanguage = g_Config.m_ClLanguagefile;
	const uint64_t LayoutRevision = CardOrderModel.LayoutRevision();
	if(!s_GlobalSearchCache.m_Valid || s_GlobalSearchCache.m_Search != (pModuleSearch != nullptr ? pModuleSearch : "") || s_GlobalSearchCache.m_Language != (pLanguage != nullptr ? pLanguage : "") || s_GlobalSearchCache.m_Sixup != Client()->IsSixup() || s_GlobalSearchCache.m_LayoutRevision != LayoutRevision)
	{
		s_GlobalSearchCache.m_Search = pModuleSearch != nullptr ? pModuleSearch : "";
		s_GlobalSearchCache.m_Language = pLanguage != nullptr ? pLanguage : "";
		s_GlobalSearchCache.m_Sixup = Client()->IsSixup();
		s_GlobalSearchCache.m_LayoutRevision = LayoutRevision;
		CollectGlobalSearchResults(pModuleSearch, s_GlobalSearchCache.m_Sixup, CardOrderModel, s_GlobalSearchCache.m_Results);
		auto &vResults = s_GlobalSearchCache.m_Results.m_vAllVisibleCards;
		const TConfigAddresses Addresses = SettingsSearchConfigAddresses(ConfigManager());
		s_GlobalSearchCache.m_Highlights.clear();
		for(const auto &Card : vResults)
		{
			std::vector<SSettingsSearchHighlightTarget> vTargets;
			for(const auto &Setting : Card.m_vMatchedSettings)
			{
				const auto Address = Addresses.find(Setting.m_ConfigName);
				vTargets.push_back({Setting.m_DisplayLabel, Address == Addresses.end() ? nullptr : Address->second});
			}
			auto Highlight = std::make_shared<CSettingsSearchHighlight>();
			Highlight->SetTargets(std::move(vTargets));
			s_GlobalSearchCache.m_Highlights.emplace(Card.m_pStableId, std::move(Highlight));
		}

		std::vector<qm_card_order::SEntry> vEntries;
		vEntries.reserve(vResults.size() + 1);
		// 搜索输入卡片不再进入卡组：它脱离滚动流手动绘制（默认居中，搜索后置顶）。
		vEntries.push_back({"deck:global-search-empty", "global-search", 0, -1});
		for(size_t Index = 0; Index < vResults.size(); ++Index)
		{
			// 搜索结果卡片使用半宽：交替放入左右两列（0=整宽保留给搜索输入与空态卡片）。
			const int ResultColumn = Index % 2 == 0 ? 1 : 2;
			vEntries.push_back({vResults[Index].m_pStableId, "global-search", ResultColumn, (int)(Index / 2)});
		}
		s_GlobalSearchCache.m_Model.SetEntries(vEntries);
		s_GlobalSearchPrewarmOrderModel.SetEntries(std::move(vEntries));
		s_GlobalSearchCache.m_Valid = true;
		++s_GlobalSearchCache.m_ResultsRevision;
	}
	const std::vector<SQmGlobalSearchCard> &SearchVisibleGlobalCards = s_GlobalSearchCache.m_Results.m_vAllVisibleCards;
	const int SearchMatchedGlobalCardCount = (int)SearchVisibleGlobalCards.size();
	if(!ReadOnly)
	{
		if(s_LiveSearchQuery != s_GlobalSearchCache.m_Search)
		{
			s_LiveSearchQuery = s_GlobalSearchCache.m_Search;
			s_GlobalSearchScrollRegion.SetScrollOffsetY(0.0f);
			s_SearchCollapsed.clear();
		}
		if(s_LiveResultsRevision != s_GlobalSearchCache.m_ResultsRevision || m_SettingsSearchSession.Query() != s_GlobalSearchCache.m_Search)
		{
			std::vector<std::string> vResultIds;
			for(const auto &Card : SearchVisibleGlobalCards)
				vResultIds.emplace_back(Card.m_pStableId);
			m_SettingsSearchSession.SetResults(s_GlobalSearchCache.m_Search, std::move(vResultIds));
			s_LiveResultsRevision = s_GlobalSearchCache.m_ResultsRevision;
		}
		if(m_SettingsSearchRevealOnRefresh)
		{
			m_SettingsSearchSession.RevealSelection();
			m_SettingsSearchRevealOnRefresh = false;
		}
		for(auto &[Id, Highlight] : s_GlobalSearchCache.m_Highlights)
			Highlight->ClearMatches();
		if(m_SettingsSearchSession.PendingReveal().empty())
			s_LastRevealRequest.clear();
		else if(s_LastRevealRequest != m_SettingsSearchSession.PendingReveal())
		{
			s_LastRevealRequest = m_SettingsSearchSession.PendingReveal();
			s_SearchCollapsed[s_LastRevealRequest] = false;
			CardDeck.RequestReveal(s_LastRevealRequest.c_str());
		}
	}
	s_GlobalSearchFunctionCardLayout = qm_card_catalog::ResolveFunctionCardLayoutState(GameClient()->TClientComponent().GetFavoriteMaps().size());
	qm_card_catalog::SQmCardBuildContext StandardPrepare;
	StandardPrepare.m_pMenus = this;
	StandardPrepare.m_ReadOnly = ReadOnly;
	StandardPrepare.m_Page = Page;
	StandardPrepare.m_Metrics = Metrics;
	StandardPrepare.m_UiContext = SearchCtx;
	StandardPrepare.m_pScrollRegion = ReadOnly ? nullptr : &s_GlobalSearchScrollRegion;
	std::vector<const char *> vStandardIds;
	vStandardIds.reserve(SearchVisibleGlobalCards.size());
	for(const auto &Card : SearchVisibleGlobalCards)
		vStandardIds.push_back(Card.m_pStableId);
	uint64_t CardLayoutRevision = str_quickhash("global-search");
	CardLayoutRevision ^= qm_card_catalog::QmCardRenderHook::PrepareStandardCards(StandardPrepare, vStandardIds);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ qm_card_catalog::QmCardRenderHook::PrepareTClientCards(StandardPrepare, vStandardIds);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ str_quickhash(pModuleSearch != nullptr ? pModuleSearch : "");
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ LayoutRevision;
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ (Client()->IsSixup() ? 1u : 0u);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ (ReadOnly ? 1u : 0u);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ qm_card_catalog::MeasureModuleCardsRevision(s_GlobalSearchFunctionCardLayout);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ TranslationTestLayoutRevision();
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ qm_card_catalog::MeasureContentRevision(g_Localization.Languages().size(), GameClient()->m_MenuBackground.GetThemes().size());
	if(std::any_of(SearchVisibleGlobalCards.begin(), SearchVisibleGlobalCards.end(), [](const SQmGlobalSearchCard &Card) { return str_startswith(Card.m_pStableId, "deck:controls-") != nullptr; }))
		CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ qm_card_catalog::QmCardRenderHook::PrepareControlsCards(this, MainView.w, ReadOnly, ReadOnly ? nullptr : &s_GlobalSearchScrollRegion);
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ s_GlobalSearchFunctionCardLayout.m_BlockWordsRevision;
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ s_GlobalSearchFunctionCardLayout.m_KeywordRulesRevision;
	CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ s_GlobalSearchFunctionCardLayout.m_FavoriteMapsRevision;
	for(const SQmGlobalSearchCard &Card : SearchVisibleGlobalCards)
		CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ str_quickhash(Card.m_pStableId);
	const uint64_t DefinitionsRevision = ResolveSettingsCardDefinitionsRevision(m_SettingsCardDeckDisplayCycle, m_MenuTextPoolGeneration, MainView.w, CardLayoutRevision);

	const SQmSettingsCardStyle CardStyle = QmSettingsCardStyle(UiScale);
	s_GlobalSearchCardBuild = {};
	s_GlobalSearchCardBuild.m_pMenus = this;
	s_GlobalSearchCardBuild.m_ReadOnly = ReadOnly;
	s_GlobalSearchCardBuild.m_Page = Page;
	s_GlobalSearchCardBuild.m_Metrics = Metrics;
	s_GlobalSearchCardBuild.m_LabelWidth = ResolveSettingsCardLabelWidth(Page.m_TwoColumns ? Page.m_aColumns[0].w : Page.m_ContentViewport.w, Metrics);
	s_GlobalSearchCardBuild.m_UiContext = SearchCtx;
	s_GlobalSearchCardBuild.m_pScrollRegion = ReadOnly ? nullptr : &s_GlobalSearchScrollRegion;
	s_GlobalSearchCardBuild.m_Padding = CardStyle.m_Padding;
	s_GlobalSearchCardBuild.m_CornerRadius = CardStyle.m_CornerRadius;
	s_GlobalSearchCardBuild.m_pCollapsed = s_aGlobalSearchCollapsed.data();
	s_GlobalSearchCardBuild.m_pCollapseButtons = s_aGlobalSearchCollapseButtons.data();
	s_GlobalSearchCardBuild.m_pToggleCollapsed = ToggleSearchCollapsed;
	s_GlobalSearchCardBuild.m_pToggleCollapsedUser = s_aGlobalSearchCollapsed.data();
	s_GlobalSearchCardBuild.m_pFunctionLayout = &s_GlobalSearchFunctionCardLayout;

	const bool HasSearchQuery = pModuleSearch != nullptr && str_utf8_skip_whitespaces(pModuleSearch)[0] != '\0';
	// s_GlobalSearchCardBuild 是函数内静态对象，直接引用即可，无需附加捕获。
	const auto BuildDefinitions = [this, UiScale, BodySize, SmallSize, LineHeight, LineSpacing, SearchMatchedGlobalCardCount, HasSearchQuery, ReadOnly, SearchCtx, &SearchVisibleGlobalCards](std::vector<SSettingsCardDefinition> &vCards) {
		vCards.reserve(SearchMatchedGlobalCardCount + 1);
		// 搜索输入卡片已脱离卡组，在 RenderCached 之前手动绘制并定位。

		SSettingsCardDefinition EmptyCard;
		EmptyCard.m_Spec = {"deck:global-search-empty", Localize("Search"), qm_card_registry::ResolveLocalizedDescription("deck:global-search-results")};
		EmptyCard.m_Measure = [LineHeight](float) { return LineHeight; };
		EmptyCard.m_Render = [this, SmallSize, LineHeight](CUIRect Content) {
			CUIRect Row;
			Content.HSplitTop(LineHeight, &Row, &Content);
			DoSettingsMenuLabel(SETTINGS_SEARCH, -1, -1, "qmclient-search-no-matching-features", &Row, Localize("No matching features found. Try other keywords"), SmallSize, TEXTALIGN_ML, {}, (int)Row.w);
		};
		EmptyCard.m_IsVisible = [HasSearchQuery, SearchMatchedGlobalCardCount] { return HasSearchQuery && SearchMatchedGlobalCardCount == 0; };
		vCards.push_back(std::move(EmptyCard));

		for(const SQmGlobalSearchCard &Card : SearchVisibleGlobalCards)
		{
			SSettingsCardDefinition Definition;
			if(!qm_card_catalog::BuildCard(s_GlobalSearchCardBuild, Card.m_pStableId, Definition))
			{
				const qm_card_registry::SCardDefault *pDefault = qm_card_registry::FindByStableId(Card.m_pStableId);
				Definition.m_Spec = {Card.m_pStableId, pDefault != nullptr && pDefault->m_pTitle != nullptr ? Localize(pDefault->m_pTitle) : Localize("Global card"), qm_card_registry::ResolveLocalizedDescription(Card.m_pStableId)};
				// 兜底卡无法复用原页面的卡片构建器（构建闭包绑定在各自页面），
				// 退化为「描述 + 定位按钮」的导航卡，而不是只有一行定位。
				Definition.m_Measure = [LineHeight, LineSpacing](float) { return LineHeight + LineSpacing * 0.65f + LineHeight; };
				CButtonContainer *pOpenButton = &s_GlobalSearchActionButtons[std::string("open:") + Card.m_pStableId];
				Definition.m_Render = [this, Description = Card.m_Description, Target = Card.m_Target, pOpenButton, ReadOnly, LineHeight, LineSpacing, SmallSize](CUIRect Content) {
					CUIRect Row;
					Content.HSplitTop(LineHeight, &Row, &Content);
					SLabelProperties DescProps;
					DescProps.m_MaxWidth = Row.w;
					DescProps.m_EllipsisAtEnd = true;
					TextRender()->TextColor(ColorRGBA(0.9f, 0.9f, 0.9f, 0.82f));
					Ui()->DoLabel(&Row, Description.c_str(), SmallSize, TEXTALIGN_ML, DescProps);
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					Content.HSplitTop(LineSpacing * 0.65f, nullptr, &Content);
					Content.HSplitTop(LineHeight, &Row, &Content);
					const char *pLabel = Localize("Locate");
					const float TextWidth = TextRender()->TextWidth(SmallSize, pLabel);
					CUIRect LocateButton = {Row.x, Row.y, std::min(Row.w, TextWidth + 16.0f), Row.h};
					if(LocateButton.w <= 0.0f)
						return;
					if(!ReadOnly && Ui()->DoButtonLogic(pOpenButton, 0, &LocateButton, BUTTONFLAG_LEFT))
					{
						NavigateToSettingsCard(Target);
						Ui()->ReleaseActiveTextInput(&m_GlobalCardSearchInput);
						m_GlobalCardSearchInput.Deactivate();
						return;
					}
					// 完整按钮组件：圆角底 + 居中文本，而非纯文本。
					const bool Hovered = !ReadOnly && Ui()->MouseHovered(&LocateButton);
					const ColorRGBA ChromeColor(1.0f, 1.0f, 1.0f, Hovered ? 0.28f : 0.18f);
					DrawRoundedSurface(Ui(), LocateButton, ChromeColor, ChromeColor, 5.0f);
					Ui()->DoLabel(&LocateButton, pLabel, SmallSize, TEXTALIGN_MC);
				};
				vCards.push_back(std::move(Definition));
				continue;
			}
			const std::string StableId = Card.m_pStableId;
			const auto Highlight = s_GlobalSearchCache.m_Highlights.at(StableId);
			const auto AfterRender = [this, StableId, Highlight, ReadOnly] {
				if(ReadOnly || m_SettingsSearchSession.PendingReveal() != StableId)
					return;
				const auto &vMatches = Highlight->Matches();
				if(!vMatches.empty())
				{
					const auto Match = std::min_element(vMatches.begin(), vMatches.end(), [](const auto &A, const auto &B) { return A.m_TargetIndex < B.m_TargetIndex; });
					s_GlobalSearchScrollRegion.AddRect(Match->m_Rect, true);
				}
				m_SettingsSearchSession.FinishReveal();
			};
			if(Definition.m_Render)
			{
				Definition.m_Render = [this, Render = std::move(Definition.m_Render), Highlight, AfterRender, ReadOnly](CUIRect Content) {
					CUiScopedSettingsSearchHighlight Scope(Ui(), ReadOnly ? nullptr : Highlight.get());
					Render(Content);
					AfterRender();
				};
			}
			if(Definition.m_RenderMeasured)
			{
				Definition.m_RenderMeasured = [this, Render = std::move(Definition.m_RenderMeasured), Highlight, AfterRender, ReadOnly](CUIRect &Content) {
					CUiScopedSettingsSearchHighlight Scope(Ui(), ReadOnly ? nullptr : Highlight.get());
					Render(Content);
					AfterRender();
				};
			}
			// 命中卡片默认展开；搜索页的折叠不会写回分类页。
			Definition.m_IsCollapsed = [StableId] { return s_SearchCollapsed[StableId]; };
			CButtonContainer *pCollapseButton = &s_GlobalSearchActionButtons[std::string("collapse:") + StableId];
			Definition.m_PreLayoutHeaderInput = [this, StableId, pCollapseButton, ReadOnly](const SSettingsCardFrame &Frame, bool Collapsed) {
				if(ReadOnly || !Ui()->DoButtonLogic(pCollapseButton, 0, &Frame.m_HandleRect, BUTTONFLAG_LEFT))
					return false;
				s_SearchCollapsed[StableId] = !Collapsed;
				return true;
			};
			Definition.m_HeaderAction = [SearchCtx, pCollapseButton](const SSettingsCardFrame &Frame, bool Collapsed) {
				RenderSettingsCardCollapseButton(SearchCtx, Frame.m_HandleRect, Collapsed, 1.0f, pCollapseButton);
			};
			CButtonContainer *pLocateButton = &s_GlobalSearchActionButtons[std::string("locate:") + Card.m_pStableId];
			Definition.m_LeadingHeaderActionWidth = TextRender()->TextWidth(SmallSize, Localize("Locate")) + 16.0f * UiScale;
			Definition.m_LeadingHeaderAction = [this, Target = Card.m_Target, pLocateButton, ReadOnly, UiScale, SmallSize, SearchCtx](const SSettingsCardFrame &Frame, bool) {
				if(ReadOnly)
					return;
				// 定位使用 Deck 预留的最左操作区，不再覆盖宽度或说明按钮。
				const char *pLabel = Localize("Locate");
				const CUIRect LocateButton = Frame.m_LeadingHeaderActionRect;
				if(LocateButton.w <= 0.0f || LocateButton.h <= 0.0f)
					return;
				if(Ui()->DoButtonLogic(pLocateButton, 0, &LocateButton, BUTTONFLAG_LEFT))
				{
					NavigateToSettingsCard(Target);
					Ui()->ReleaseActiveTextInput(&m_GlobalCardSearchInput);
					m_GlobalCardSearchInput.Deactivate();
					return;
				}
				// 完整按钮组件：圆角底 + 居中文本，而非纯文本。
				const bool Hovered = Ui()->MouseHovered(&LocateButton);
				const float Radius = std::min(ui_token::radius::TIGHT * UiScale, std::min(LocateButton.w, LocateButton.h) * 0.25f);
				const ColorRGBA ChromeColor(1.0f, 1.0f, 1.0f, Hovered ? 0.28f : 0.18f);
				if(LocateButton.w < TextRender()->TextWidth(SmallSize, pLabel) + 16.0f * UiScale)
				{
					RenderSettingsCardHeaderIcon(SearchCtx, LocateButton, EQmIcon::ARROW_RIGHT, FontIcons::FONT_ICON_CHEVRON_RIGHT, 1.0f, pLocateButton);
					GameClient()->m_Tooltips.DoSmallToolTip(pLocateButton, &LocateButton, pLabel, SmallSize);
				}
				else
				{
					DrawRoundedSurface(Ui(), LocateButton, ChromeColor, ChromeColor, Radius);
					Ui()->DoLabel(&LocateButton, pLabel, SmallSize, TEXTALIGN_MC);
				}
			};
			vCards.push_back(std::move(Definition));
		}
	};

	const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_OUTER}, UiScale, 0.0f);
	const CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);
	const SCardMotionSpec Motion = SettingsCardMotionSpec();

	// —— 搜索输入卡片：脱离卡组手动绘制 ——
	// 无搜索内容时在视口内垂直居中；输入内容后置顶并把下方空间让位给结果卡片，
	// 两种状态间的过渡沿用卡组让位动画的时长与缓动。
	static CButtonContainer s_GlobalSearchInputCollapseButton;
	static bool s_GlobalSearchInputCollapsed = false;
	if(!ReadOnly && m_FocusSettingsSearchOnEntry)
		s_GlobalSearchInputCollapsed = false;
	const float InputContentHeight = s_GlobalSearchInputCollapsed ? 0.0f : 2.0f * LineHeight + LineSpacing;
	const SSettingsCardSpec InputSpec{"deck:global-search-input", Localize("Feature Search"), qm_card_registry::ResolveLocalizedDescription("deck:global-search-input")};
	const float InputChromeHeight = BuildSettingsCardFrame({0.0f, 0.0f, Page.m_ContentViewport.w, 0.0f}, InputSpec, InputContentHeight, UiScale).m_Rect.h;
	const float CenteredOffsetY = std::max(0.0f, (Page.m_ContentViewport.h - InputChromeHeight) * 0.5f);
	const float TargetOffsetY = HasSearchQuery ? 0.0f : CenteredOffsetY;
	float InputOffsetY = TargetOffsetY;
	CUiV2AnimationRuntime *pSearchAnimRuntime = ReadOnly ? nullptr : &GameClient()->UiRuntimeV2()->AnimRuntime();
	if(pSearchAnimRuntime != nullptr)
	{
		const uint64_t InputOffsetKey = BuildUiAnimNodeKey(str_quickhash("global-search"), str_quickhash("global-search-input-offset"));
		InputOffsetY = ResolveUiAnimValue(*pSearchAnimRuntime, InputOffsetKey, EUiAnimProperty::POS_Y, TargetOffsetY, Motion.m_ReflowDuration, EEasing::EASE_OUT);
	}
	// 进入「搜索」页第一帧自动聚焦输入框：同时建立 ActiveItem 与 CLineInput 激活态，
	// 之后由 DoEditBox 的激活维持逻辑接管；一次性消费，点击其他控件可正常移走焦点。
	if(!ReadOnly && m_FocusSettingsSearchOnEntry && !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && !s_GlobalSearchInputCollapsed)
	{
		m_FocusSettingsSearchOnEntry = false;
		Ui()->SetActiveItem(&m_GlobalCardSearchInput);
		m_GlobalCardSearchInput.Activate(EInputPriority::UI);
		m_GlobalCardSearchInput.SelectAll();
	}
	{
		const CUIRect InputSlot{Page.m_ContentViewport.x, Page.m_ContentViewport.y + InputOffsetY, Page.m_ContentViewport.w, 0.0f};
		SSettingsCardVisualState InputVisualState;
		InputVisualState.m_Collapsed = s_GlobalSearchInputCollapsed;
		InputVisualState.m_ClipContent = true;
		const auto RenderInputContent = [this, UiScale, BodySize, SmallSize, LineHeight, LineSpacing, SearchMatchedGlobalCardCount, ReadOnly](CUIRect Content) {
			if(Content.h <= 0.0f || Content.w <= 0.0f)
				return;
			CUIRect Row;
			Content.HSplitTop(LineHeight, &Row, &Content);
			IUiContext InputCtx = SettingsUiContext("settings_global_search", UiScale);
			if(ReadOnly)
			{
				InputCtx.m_pAnim = nullptr;
				InputCtx.m_pTree = nullptr;
			}
			ui_widget::InputField(InputCtx, &m_GlobalCardSearchInput, Row, BodySize, !ReadOnly && !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
			Content.HSplitTop(LineSpacing * 0.65f, nullptr, &Content);
			Content.HSplitTop(LineHeight, &Row, &Content);
			char aSearchHint[64];
			str_format(aSearchHint, sizeof(aSearchHint), Localize("Found %d global cards"), SearchMatchedGlobalCardCount);
			TextRender()->TextColor(ColorRGBA(0.9f, 0.9f, 0.9f, 0.82f));
			Ui()->DoLabel(&Row, aSearchHint, SmallSize, TEXTALIGN_ML);
			if(Row.w > 340.0f * UiScale)
			{
#ifdef CONF_PLATFORM_MACOSX
				Ui()->DoLabel(&Row, "⌘F   ↑/↓   Enter   Esc", SmallSize, TEXTALIGN_MR);
#else
				Ui()->DoLabel(&Row, "Ctrl+F   ↑/↓   Enter   Esc", SmallSize, TEXTALIGN_MR);
#endif
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		};
		const auto InputHeaderAction = [this, ReadOnly, UiScale, SearchCtx](const SSettingsCardFrame &Frame, bool) mutable {
			if(ReadOnly)
				return;
			if(Ui()->DoButtonLogic(&s_GlobalSearchInputCollapseButton, 0, &Frame.m_HandleRect, BUTTONFLAG_LEFT))
				s_GlobalSearchInputCollapsed = !s_GlobalSearchInputCollapsed;
			RenderSettingsCardCollapseButton(SearchCtx, Frame.m_HandleRect, s_GlobalSearchInputCollapsed, 1.0f, &s_GlobalSearchInputCollapseButton);
		};
		SettingsCard(SearchCtx, InputSlot, InputSpec, InputVisualState, SettingsCardDeckVisualOptions(), [InputContentHeight](float) { return InputContentHeight; }, RenderInputContent, InputHeaderAction);
	}

	// 结果卡组从输入卡片底部（含动画中的位置）开始，让位动画期间跟随卡片一起上移。
	SSettingsPageLayoutFrame DeckPage = Page;
	{
		const float DeckTopDelta = InputOffsetY + InputChromeHeight + Page.m_CardGap;
		const auto ShiftRectDown = [DeckTopDelta](CUIRect &Rect) {
			Rect.y += DeckTopDelta;
			Rect.h = std::max(0.0f, Rect.h - DeckTopDelta);
		};
		ShiftRectDown(DeckPage.m_ScrollViewport);
		ShiftRectDown(DeckPage.m_UnreservedScrollViewport);
		ShiftRectDown(DeckPage.m_ContentViewport);
		ShiftRectDown(DeckPage.m_aColumns[0]);
		ShiftRectDown(DeckPage.m_aColumns[1]);
	}

	SSettingsCardDeckInput InputState;
	InputState.m_MouseX = ReadOnly ? 0.0f : Ui()->MouseX();
	InputState.m_MouseY = ReadOnly ? 0.0f : Ui()->MouseY();
	InputState.m_MousePressed = !ReadOnly && Ui()->MouseButtonClicked(0);
	InputState.m_MouseDown = !ReadOnly && Ui()->MouseButton(0);
	InputState.m_MouseReleased = !ReadOnly && !InputState.m_MouseDown && Ui()->LastMouseButton(0);
	InputState.m_CtrlPressed = false;
	// 搜索结果按匹配顺序排列，不修改分类页的持久布局。
	InputState.m_AllowHeaderDrag = false;
	InputState.m_pFocusedStableId = ReadOnly ? nullptr : m_SettingsSearchSession.Selected().c_str();
	InputState.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
	InputState.m_pScrollParams = ReadOnly ? nullptr : &ScrollParams;
	CardDeck.RenderCached(SearchCtx, DeckPage, "global-search", DefinitionsRevision, BuildDefinitions, DeckOrderModel, ReadOnly ? nullptr : &s_GlobalSearchScrollRegion, InputState, Motion, SettingsCardDeckVisualOptions());
}

bool CMenus::HandleSettingsSearchInput(const IInput::CEvent &Event)
{
	if(!(Event.m_Flags & IInput::FLAG_PRESS) || !IsSettingsPageActive() || m_Popup != POPUP_NONE || Ui()->IsPopupOpen() || GameClient()->m_GameConsole.IsActive() || GameClient()->m_KeyBinder.IsActive() || Input()->HasComposition())
		return false;
	CLineInput *pActiveInput = CLineInput::GetActiveInput();
	if(Event.m_Key == KEY_F && Input()->ModifierIsPressed() && (pActiveInput == nullptr || pActiveInput == &m_GlobalCardSearchInput))
	{
		g_Config.m_UiSettingsPage = SETTINGS_SEARCH;
		m_FocusSettingsSearchOnEntry = true;
		m_ReturnToSettingsSearch = false;
		return true;
	}
	if(g_Config.m_UiSettingsPage != SETTINGS_SEARCH)
	{
		if(m_ReturnToSettingsSearch && g_Config.m_UiSettingsPage != m_SettingsSearchLocatedPage)
			m_ReturnToSettingsSearch = false;
		if(Event.m_Key != KEY_ESCAPE || !m_ReturnToSettingsSearch)
			return false;
		if(pActiveInput != nullptr)
		{
			Ui()->ReleaseActiveTextInput(pActiveInput);
			pActiveInput->Deactivate();
			return true;
		}
		if(Ui()->ActiveItem() != nullptr)
			return false;
		g_Config.m_UiSettingsPage = SETTINGS_SEARCH;
		m_ReturnToSettingsSearch = false;
		return true;
	}
	if(Event.m_Key == KEY_ESCAPE && pActiveInput != nullptr && pActiveInput != &m_GlobalCardSearchInput)
	{
		Ui()->ReleaseActiveTextInput(pActiveInput);
		pActiveInput->Deactivate();
		return true;
	}
	if((pActiveInput != nullptr && pActiveInput != &m_GlobalCardSearchInput) || (Ui()->ActiveItem() != nullptr && Ui()->ActiveItem() != &m_GlobalCardSearchInput))
		return false;
	if(Event.m_Key == KEY_ESCAPE)
	{
		if(pActiveInput == &m_GlobalCardSearchInput)
		{
			Ui()->ReleaseActiveTextInput(&m_GlobalCardSearchInput);
			m_GlobalCardSearchInput.Deactivate();
			return true;
		}
		if(m_GlobalCardSearchInput.GetString()[0] != '\0')
		{
			m_GlobalCardSearchInput.Clear();
			m_FocusSettingsSearchOnEntry = true;
			return true;
		}
		return false;
	}
	if(Input()->ModifierIsPressed() || Input()->AltIsPressed() || Input()->ShiftIsPressed())
		return false;
	if(Event.m_Key == KEY_UP || Event.m_Key == KEY_DOWN)
		return m_SettingsSearchSession.Query() == m_GlobalCardSearchInput.GetString() && m_SettingsSearchSession.MoveSelection(Event.m_Key == KEY_DOWN ? 1 : -1);
	if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
	{
		if(m_SettingsSearchSession.Query() != m_GlobalCardSearchInput.GetString())
			m_SettingsSearchRevealOnRefresh = true;
		else if(!m_SettingsSearchSession.RevealSelection())
			return false;
		Ui()->ReleaseActiveTextInput(&m_GlobalCardSearchInput);
		m_GlobalCardSearchInput.Deactivate();
		return true;
	}
	return false;
}
