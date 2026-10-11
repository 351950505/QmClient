#include <base/log.h>
#include <base/math.h>
#include <base/perf_timer.h>
#include <base/str.h>
#include <base/system.h>
#include <base/types.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/image.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/config_tags.h>
#include <engine/shared/jobs.h>
#include <engine/shared/json.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/warning.h>

#include <game/client/QmUi/QmCardOrderModel.h>
#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/QmDropdown.h>
#include <game/client/QmUi/SecondaryPanel.h>
#include <game/client/QmUi/SettingsCard.h>
#include <game/client/QmUi/SettingsFontSelection.h>
#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/UiButtons.h>
#include <game/client/QmUi/UiForms.h>
#include <game/client/QmUi/UiNavigation.h>
#include <game/client/QmUi/UiSurface.h>
#include <game/client/QmUi/UiSurfaceText.h>
#include <game/client/QmUi/cards/QmCardCatalog.h>
#include <game/client/QmUi/cards/QmCardCatalogTClientInternal.h>
#include <game/client/QmUi/cards/TClientConfigSource.h>
#include <game/client/animstate.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/qmclient/font_download_storage.h>
#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/components/section_loader.h>
#include <game/client/components/skins.h>
#include <game/client/components/tclient/bindchat.h>
#include <game/client/components/tclient/bindwheel.h>
#include <game/client/components/tclient/trails.h>
#include <game/client/gameclient.h>
#include <game/client/qm_icon.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace FontIcons;
using namespace qm_tclient_cards;

qm_card_catalog::STClientCardResult CMenus::RunTClientConfigsCard(const qm_card_catalog::SQmCardBuildContext &Ctx, const char *pStableId, CUIRect &Content, qm_card_catalog::ETClientCardPass Pass)
{
	CUIRect MainView = Ctx.m_Page.m_ContentViewport;
	ApplyTClientContentMetrics(Ctx.m_Metrics);
	CPerfTimer RenderTimer;
	const bool ReadOnly = Ctx.m_ReadOnly || Pass != qm_card_catalog::ETClientCardPass::RENDER;
	const float UiScale = SettingsPageUiScale(MainView.w);
	const SSettingsContentMetrics ConfigMetrics = ResolveSettingsContentMetrics(MainView.w);
	std::unique_ptr<CUiRenderOnlyGuard> pRenderOnlyGuard;
	if(ReadOnly && !Ui()->RenderOnly())
		pRenderOnlyGuard = std::make_unique<CUiRenderOnlyGuard>(Ui());
	// hi hello, this is a relatively self-contained mess, sorry if you're forking or need to modify this -Tater
	// 你好, 这是一个相对独立的混乱，如果你要分叉或需要修改它，抱歉 -Tater
	struct SIntStage
	{
		int m_Value;
	};
	struct SStrStage
	{
		std::string m_Value;
	};
	struct SColStage
	{
		unsigned m_Value;
	};
	enum class EConfigSource
	{
		DDNET,
		TCLIENT,
		QM,
	};
	static std::unordered_map<const SConfigVariable *, SIntStage> s_StagedInts;
	static std::unordered_map<const SConfigVariable *, SStrStage> s_StagedStrs;
	static std::unordered_map<const SConfigVariable *, SColStage> s_StagedCols;

	struct SIntState
	{
		CLineInputNumber m_Input;
		int m_LastValue = 0;
		bool m_Inited = false;
	};
	struct SStrState
	{
		std::vector<char> m_vBuffer;
		CLineInput m_Input;
		bool m_Inited = false;
	};
	struct SColState
	{
		unsigned m_LastValue = 0;
		unsigned m_Working = 0;
		bool m_Inited = false;
	};
	static std::unordered_map<const SConfigVariable *, SIntState> s_IntInputs;
	static std::unordered_map<const SConfigVariable *, SStrState> s_StrInputs;
	static std::unordered_map<const SConfigVariable *, SColState> s_ColInputs;
	static std::unordered_map<const SConfigVariable *, SIntState> s_ReadOnlyIntInputs;
	static std::unordered_map<const SConfigVariable *, SStrState> s_ReadOnlyStrInputs;
	static std::unordered_map<const SConfigVariable *, SColState> s_ReadOnlyColInputs;
	auto &IntInputs = ReadOnly ? s_ReadOnlyIntInputs : s_IntInputs;
	auto &StrInputs = ReadOnly ? s_ReadOnlyStrInputs : s_StrInputs;
	auto &ColInputs = ReadOnly ? s_ReadOnlyColInputs : s_ColInputs;

	auto ClearStagedAndCaches = [&]() {
		s_StagedInts.clear();
		s_StagedStrs.clear();
		s_StagedCols.clear();
		s_IntInputs.clear();
		s_StrInputs.clear();
		s_ColInputs.clear();
	};
	auto SortStagedKeys = [](auto &Staged) {
		std::vector<const SConfigVariable *> vKeys;
		vKeys.reserve(Staged.size());
		for(const auto &Entry : Staged)
			vKeys.push_back(Entry.first);
		std::sort(vKeys.begin(), vKeys.end(), [](const SConfigVariable *pLeft, const SConfigVariable *pRight) {
			return str_comp(pLeft->m_pScriptName, pRight->m_pScriptName) < 0;
		});
		return vKeys;
	};

	size_t ChangesCount = 0;

	static CLineInputBuffered<128> s_SearchInput;
	static int s_TcUiTagVisual = 0;
	static int s_TcUiTagHud = 0;
	static int s_TcUiTagInput = 0;
	static int s_TcUiTagChat = 0;
	static int s_TcUiTagAudio = 0;
	static int s_TcUiTagAutomation = 0;
	static int s_TcUiTagSocial = 0;
	static int s_TcUiTagCamera = 0;
	static int s_TcUiTagGameplay = 0;
	static int s_TcUiTagMisc = 0;

	ChangesCount = s_StagedInts.size() + s_StagedStrs.size() + s_StagedCols.size();
	auto RenderActions = [&](CUIRect &Content, bool Draw) {
		CPerfTimer ActionsTimer;
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("Changes: %d"), (int)ChangesCount);
		const std::array<const char *, 3> aLabels = {aBuf, Localize("Apply Changes"), Localize("Clear Changes")};
		std::array<SSettingsToggleSize, 3> aSizes;
		for(size_t Index = 0; Index < aLabels.size(); ++Index)
			aSizes[Index] = ResolveSettingsToggleButtonSize(TextRender()->TextWidth(TCLIENT_BODY_FONT_SIZE, aLabels[Index]), TCLIENT_BODY_FONT_SIZE, LineSize, MarginSmall * 2.0f, MarginSmall);
		auto Grid = ResolveSettingsToggleGrid(Content.w, aSizes, MarginSmall);
		for(size_t Index = 0; Index < aLabels.size(); ++Index)
		{
			const float Width = std::max(1.0f, Grid.m_aColumnWidths[Index % Grid.m_Columns] - MarginSmall * 4.0f);
			aSizes[Index].m_Height = std::max(LineSize, TextRender()->TextBoundingBox(TCLIENT_BODY_FONT_SIZE, aLabels[Index], -1, Width).m_H + MarginSmall * 2.0f);
		}
		Grid.SetItemHeights(aSizes);
		CUIRect Area;
		Content.HSplitTop(Grid.Height(), &Area, &Content);
		if(!Draw)
			return;
		const CUIRect Counter = Grid.Cell(Area, 0);
		const CUIRect ApplyBtn = Grid.Cell(Area, 1);
		const CUIRect ClearBtn = Grid.Cell(Area, 2);
		static CButtonContainer s_ApplyBtn, s_ClearBtn;
		const auto ActionButton = [&](CButtonContainer &Button, const CUIRect &Rect, const char *pTextId, const char *pLabel, bool Primary) {
			const bool Enabled = ChangesCount > 0 && !ReadOnly && !m_MenuTextPlanCollecting;
			const IUiContext Context = SettingsUiContext("settings-config-actions", UiScale);
			ui_widget::SButtonSurfaceOptions Options;
			Options.m_Enabled = Enabled;
			Options.m_Role = Primary ? EUiButtonRole::PRIMARY : EUiButtonRole::SECONDARY;
			Options.m_Radius *= UiScale;
			ColorRGBA Fill(0.0f, 0.0f, 0.0f, 0.0f);
			if(!m_MenuTextPlanCollecting)
				Fill = ui_widget::DrawButtonSurface(Context, Enabled ? &Button : nullptr, Rect, Options);
			const CUiScopedSurfaceText SurfaceText(TextRender(), Fill);
			const CUIRect Label = ResolveSettingsToggleButtonLabel(Rect, MarginSmall * 2.0f, MarginSmall);
			SLabelProperties Props;
			Props.m_MaxWidth = Label.w;
			Props.m_EnableWidthCheck = false;
			Props.m_MinimumFontSize = TCLIENT_BODY_FONT_SIZE;
			DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, pTextId, &Label, pLabel, TCLIENT_BODY_FONT_SIZE, TEXTALIGN_MC, Props);
			return Enabled && Ui()->DoButtonLogic(&Button, 0, &Rect, BUTTONFLAG_LEFT);
		};
		const bool ApplyClicked = ActionButton(s_ApplyBtn, ApplyBtn, "tclient-config-apply-changes", Localize("Apply Changes"), true);
		if(ChangesCount > 0 && ApplyClicked)
		{
			for(const SConfigVariable *pVar : SortStagedKeys(s_StagedInts))
			{
				const auto Entry = s_StagedInts.find(pVar);
				dbg_assert(Entry != s_StagedInts.end(), "missing staged int");
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %d", pVar->m_pScriptName, Entry->second.m_Value);
				Console()->ExecuteLine(aCmd);
			}
			for(const SConfigVariable *pVar : SortStagedKeys(s_StagedStrs))
			{
				const auto Entry = s_StagedStrs.find(pVar);
				dbg_assert(Entry != s_StagedStrs.end(), "missing staged string");
				const auto *pString = static_cast<const SStringConfigVariable *>(pVar);
				std::vector<char> vCommand(pString->MaxSerializedSize());
				pString->Serialize(vCommand.data(), vCommand.size(), Entry->second.m_Value.c_str());
				Console()->ExecuteLine(vCommand.data());
			}
			for(const SConfigVariable *pVar : SortStagedKeys(s_StagedCols))
			{
				const auto Entry = s_StagedCols.find(pVar);
				dbg_assert(Entry != s_StagedCols.end(), "missing staged color");
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %u", pVar->m_pScriptName, Entry->second.m_Value);
				Console()->ExecuteLine(aCmd);
			}
			ClearStagedAndCaches();
		}
		const bool ClearClicked = ActionButton(s_ClearBtn, ClearBtn, "tclient-config-clear-changes", Localize("Clear Changes"), false);
		if(ChangesCount > 0 && ClearClicked)
		{
			ClearStagedAndCaches();
		}

		const CUIRect CounterLabel = ResolveSettingsToggleButtonLabel(Counter, MarginSmall * 2.0f, MarginSmall);
		SLabelProperties CounterProps;
		CounterProps.m_MaxWidth = CounterLabel.w;
		CounterProps.m_EnableWidthCheck = false;
		CounterProps.m_MinimumFontSize = TCLIENT_BODY_FONT_SIZE;
		Ui()->DoLabel(&CounterLabel, aBuf, TCLIENT_BODY_FONT_SIZE, TEXTALIGN_ML, CounterProps);
		LogTClientPerfStageEx("tclient_configs", "actions", ETClientSettingsPerfStage::INTERACTIVE_LAYER, ActionsTimer.ElapsedMs(), false, aBuf);
	};

	// 测量和绘制消费相同的分组布局，长译文与窄卡片不另走固定列数分支。
	auto RenderFilters = [&](CUIRect &Content, bool Draw) {
		const auto TogglePass = Draw ? ESettingsToggleGroupPass::RENDER : ESettingsToggleGroupPass::LAYOUT;
		CUIRect Search;
		Content.HSplitTop(ConfigMetrics.m_InputHeight, &Search, &Content);
		Content.HSplitTop(ConfigMetrics.m_SectionGap, nullptr, &Content);
		if(Draw)
		{
			IUiContext SearchCtx = SettingsUiContext("settings_tclient_config_search", UiScale);
			s_SearchInput.SetEmptyText(Localize("Search"));
			ui_widget::InputField(SearchCtx, &s_SearchInput, Search, EditBoxFontSize, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
		}
		auto Heading = [&](const char *pId, const char *pLabel) {
			const float Height = std::max(ConfigMetrics.m_LineHeight, TextRender()->TextBoundingBox(FontSize, pLabel, -1, std::max(1.0f, Content.w)).m_H);
			CUIRect Row;
			Content.HSplitTop(Height, &Row, &Content);
			Content.HSplitTop(MarginSmall, nullptr, &Content);
			if(Draw)
				DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, pId, &Row, pLabel, FontSize, TEXTALIGN_ML);
		};
		Heading("tclient-config-filters-heading", Localize("Config Filters"));
		DoSettingsToggleGroup(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, Content,
			{
				{&g_Config.m_QmUiShowDDNet, "DDNet", Localize("DDNet")},
				{&g_Config.m_QmUiShowTClient, "TClient", Localize("TClient")},
				{&g_Config.m_QmUiShowQm, "QmClient", Localize("QmClient")},
			},
			ConfigMetrics, Draw && !ReadOnly, TogglePass);
		DoSettingsToggleGroup(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, Content,
			{
				{&g_Config.m_QmUiCompactList, "Compact", Localize("Compact")},
				{&g_Config.m_QmUiOnlyModified, "Modified", Localize("Modified")},
			},
			ConfigMetrics, Draw && !ReadOnly, TogglePass);
		Content.HSplitTop(MarginSmall, nullptr, &Content);
		Heading("tclient-config-tags-heading", Localize("Tags"));
		DoSettingsToggleGroup(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, Content,
			{
				{&s_TcUiTagVisual, "Visual", Localize("Visual")},
				{&s_TcUiTagHud, "HUD", Localize("HUD")},
				{&s_TcUiTagInput, "Input", Localize("Input")},
				{&s_TcUiTagChat, "Chat", Localize("Chat")},
				{&s_TcUiTagAudio, "Audio", Localize("Audio")},
				{&s_TcUiTagAutomation, "Auto", Localize("Auto")},
				{&s_TcUiTagSocial, "Social", Localize("Social")},
				{&s_TcUiTagCamera, "Camera", Localize("Camera")},
				{&s_TcUiTagGameplay, "Gameplay", Localize("Gameplay")},
				{&s_TcUiTagMisc, "Misc", Localize("Misc")},
			},
			ConfigMetrics, Draw && !ReadOnly, TogglePass);
		Content.HSplitTop(ConfigMetrics.m_SectionGap, nullptr, &Content);
	};

	auto RenderList = [&](CUIRect ListArea) {
		const int FlagMask = CFGFLAG_CLIENT;
		auto BuildConfigTagMask = [](const char *pScriptName) {
			unsigned int Mask = 0;
			for(EConfigTag Tag : ConfigTagsManager()->GetTagsForVariable(pScriptName))
				Mask |= 1u << static_cast<unsigned int>(Tag);
			return Mask;
		};
		static std::vector<const SConfigVariable *> s_vAllClientVars;
		if(s_vAllClientVars.empty())
		{
			auto Collector = [](const SConfigVariable *pVar, void *pUserData) {
				auto *pVec = static_cast<std::vector<const SConfigVariable *> *>(pUserData);
				pVec->push_back(pVar);
			};
			std::vector<const SConfigVariable *> vCollectedVars;
			ConfigManager()->PossibleConfigVariables("", FlagMask, Collector, &vCollectedVars);
			s_vAllClientVars = std::move(vCollectedVars);
			std::sort(s_vAllClientVars.begin(), s_vAllClientVars.end(), [](const SConfigVariable *a, const SConfigVariable *b) {
				if(a->m_ConfigDomain != b->m_ConfigDomain)
					return a->m_ConfigDomain < b->m_ConfigDomain;
				return str_comp(a->m_pScriptName, b->m_pScriptName) < 0;
			});
		}
		static std::vector<unsigned int> s_vAllClientVarTagMasks;
		if(s_vAllClientVarTagMasks.size() != s_vAllClientVars.size())
		{
			s_vAllClientVarTagMasks.resize(s_vAllClientVars.size());
			for(size_t i = 0; i < s_vAllClientVars.size(); ++i)
				s_vAllClientVarTagMasks[i] = BuildConfigTagMask(s_vAllClientVars[i]->m_pScriptName);
		}

		auto GetConfigSource = [&](const SConfigVariable *pVar) {
			// TClient 配置也使用 qm_ 前缀，先按定义清单区分来源。
			const char *pName = pVar->m_pScriptName ? pVar->m_pScriptName : "";
			if(IsTClientConfigVariable(pName) || str_startswith(pName, "tc_"))
				return EConfigSource::TCLIENT;
			if(str_startswith(pName, "qm_"))
				return EConfigSource::QM;
			return EConfigSource::DDNET;
		};

		auto SourceEnabled = [&](EConfigSource Source) {
			switch(Source)
			{
			case EConfigSource::DDNET: return g_Config.m_QmUiShowDDNet != 0;
			case EConfigSource::TCLIENT: return g_Config.m_QmUiShowTClient != 0;
			case EConfigSource::QM: return g_Config.m_QmUiShowQm != 0;
			default: return false;
			}
		};

		// Tags filter check
		auto TagEnabled = [&](EConfigTag Tag) -> bool {
			switch(Tag)
			{
			case EConfigTag::VISUAL: return s_TcUiTagVisual != 0;
			case EConfigTag::HUD: return s_TcUiTagHud != 0;
			case EConfigTag::INPUT: return s_TcUiTagInput != 0;
			case EConfigTag::CHAT: return s_TcUiTagChat != 0;
			case EConfigTag::AUDIO: return s_TcUiTagAudio != 0;
			case EConfigTag::AUTOMATION: return s_TcUiTagAutomation != 0;
			case EConfigTag::SOCIAL: return s_TcUiTagSocial != 0;
			case EConfigTag::CAMERA: return s_TcUiTagCamera != 0;
			case EConfigTag::GAMEPLAY: return s_TcUiTagGameplay != 0;
			case EConfigTag::MISC: return s_TcUiTagMisc != 0;
			default: return true;
			}
		};

		// Check if any tag filter is enabled
		bool AnyTagEnabled = s_TcUiTagVisual || s_TcUiTagHud || s_TcUiTagInput ||
				     s_TcUiTagChat || s_TcUiTagAudio || s_TcUiTagAutomation ||
				     s_TcUiTagSocial || s_TcUiTagCamera || s_TcUiTagGameplay ||
				     s_TcUiTagMisc;

		const char *pSearch = s_SearchInput.GetString();

		auto IsEffectiveDefaultVar = [&](const SConfigVariable *p) -> bool {
			if(p->m_Type == SConfigVariable::VAR_INT)
			{
				const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(p);
				auto Iter = s_StagedInts.find(p);
				int v = Iter != s_StagedInts.end() ? Iter->second.m_Value : *pInt->m_pVariable;
				return v == pInt->m_Default;
			}
			if(p->m_Type == SConfigVariable::VAR_STRING)
			{
				const SStringConfigVariable *pString = static_cast<const SStringConfigVariable *>(p);
				auto Iter = s_StagedStrs.find(p);
				const char *v = Iter != s_StagedStrs.end() ? Iter->second.m_Value.c_str() : pString->m_pStr;
				return str_comp(v, pString->m_pDefault) == 0;
			}
			if(p->m_Type == SConfigVariable::VAR_COLOR)
			{
				const SColorConfigVariable *pColor = static_cast<const SColorConfigVariable *>(p);
				auto Iter = s_StagedCols.find(p);
				unsigned v = Iter != s_StagedCols.end() ? Iter->second.m_Value : *pColor->m_pVariable;
				return v == pColor->m_Default;
			}
			return true;
		};

		auto BuildLocalizedConfigHelpText = [](const SConfigVariable *pVar) {
			const char *pHelpKey = pVar->m_pHelpLocalizeKey ? pVar->m_pHelpLocalizeKey : (pVar->m_pHelp ? pVar->m_pHelp : "");
			const char *pHelpText = pHelpKey[0] != '\0' ? Localize(pHelpKey) : "";
			char aHelp[512];
			if(pVar->m_Type == SConfigVariable::VAR_INT)
			{
				const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
				if(pInt->m_Min == pInt->m_Max)
					str_format(aHelp, sizeof(aHelp), "%s (%s: %d)", pHelpText, Localize("default"), pInt->m_Default);
				else if(pInt->m_Max == 0)
					str_format(aHelp, sizeof(aHelp), "%s (%s: %d, %s: %d)", pHelpText, Localize("default"), pInt->m_Default, Localize("minimum"), pInt->m_Min);
				else
					str_format(aHelp, sizeof(aHelp), "%s (%s: %d, %s: %d, %s: %d)", pHelpText, Localize("default"), pInt->m_Default, Localize("minimum"), pInt->m_Min, Localize("maximum"), pInt->m_Max);
			}
			else if(pVar->m_Type == SConfigVariable::VAR_COLOR)
			{
				const SColorConfigVariable *pColor = static_cast<const SColorConfigVariable *>(pVar);
				str_format(aHelp, sizeof(aHelp), "%s (%s: $%0*X)", pHelpText, Localize("default"), pColor->m_Alpha ? 8 : 6, color_cast<ColorRGBA>(ColorHSLA(pColor->m_Default, pColor->m_Alpha)).Pack(pColor->m_Alpha));
			}
			else if(pVar->m_Type == SConfigVariable::VAR_STRING)
			{
				const SStringConfigVariable *pString = static_cast<const SStringConfigVariable *>(pVar);
				str_format(aHelp, sizeof(aHelp), "%s (%s: \"%s\", %s: %d)", pHelpText, Localize("default"), pString->m_pDefault, Localize("maximum"), (int)pString->m_MaxSize - 1);
			}
			else
				str_copy(aHelp, pHelpText);
			return std::string(aHelp);
		};

		unsigned int SelectedTagMask = 0;
		for(int Tag = 1; Tag < static_cast<int>(EConfigTag::NUM_TAGS); ++Tag)
		{
			const EConfigTag TagValue = static_cast<EConfigTag>(Tag);
			if(TagEnabled(TagValue))
				SelectedTagMask |= 1u << static_cast<unsigned int>(TagValue);
		}
		const unsigned int MiscTagMask = 1u << static_cast<unsigned int>(EConfigTag::MISC);
		const int DomainMask = (g_Config.m_QmUiShowDDNet != 0 ? 1 : 0) |
				       (g_Config.m_QmUiShowTClient != 0 ? 2 : 0) |
				       (g_Config.m_QmUiShowQm != 0 ? 4 : 0);
		static std::vector<const SConfigVariable *> s_vCandidateConfigs;
		static std::string s_CachedConfigSearch;
		static int s_CachedConfigDomainMask = -1;
		static unsigned int s_CachedConfigTagMask = 0;
		static size_t s_CachedConfigVarCount = 0;
		static uint64_t s_CachedConfigLanguageHash = 0;
		const uint64_t ConfigLanguageHash = str_quickhash(g_Config.m_ClLanguagefile);
		if(s_CachedConfigSearch != (pSearch ? pSearch : "") ||
			s_CachedConfigDomainMask != DomainMask ||
			s_CachedConfigTagMask != SelectedTagMask ||
			s_CachedConfigVarCount != s_vAllClientVars.size() ||
			s_CachedConfigLanguageHash != ConfigLanguageHash)
		{
			s_vCandidateConfigs.clear();
			s_vCandidateConfigs.reserve(s_vAllClientVars.size());
			for(size_t i = 0; i < s_vAllClientVars.size(); ++i)
			{
				const SConfigVariable *pVar = s_vAllClientVars[i];
				if(!SourceEnabled(GetConfigSource(pVar)))
					continue;
				if(pSearch && pSearch[0])
				{
					const char *pName = pVar->m_pScriptName ? pVar->m_pScriptName : "";
					const char *pHelp = pVar->m_pHelp ? pVar->m_pHelp : "";
					std::string LocalizedHelp = BuildLocalizedConfigHelpText(pVar);
					if(!str_find_nocase(pName, pSearch) && !str_find_nocase(pHelp, pSearch) && !str_find_nocase(LocalizedHelp.c_str(), pSearch))
						continue;
				}
				if(AnyTagEnabled)
				{
					const unsigned int VarTagMask = s_vAllClientVarTagMasks[i];
					if((VarTagMask & SelectedTagMask) == 0 && !(VarTagMask == 0 && (SelectedTagMask & MiscTagMask) != 0))
						continue;
				}
				s_vCandidateConfigs.push_back(pVar);
			}
			s_CachedConfigSearch = pSearch ? pSearch : "";
			s_CachedConfigDomainMask = DomainMask;
			s_CachedConfigTagMask = SelectedTagMask;
			s_CachedConfigVarCount = s_vAllClientVars.size();
			s_CachedConfigLanguageHash = ConfigLanguageHash;
		}
		// 搜索和标签候选可缓存；是否为默认值取决于暂存值及其他卡片的实时修改。
		static std::vector<const SConfigVariable *> s_vModifiedConfigs;
		if(g_Config.m_QmUiOnlyModified)
		{
			s_vModifiedConfigs.clear();
			for(const SConfigVariable *pVar : s_vCandidateConfigs)
				if(!IsEffectiveDefaultVar(pVar))
					s_vModifiedConfigs.push_back(pVar);
		}
		const auto &vpFiltered = g_Config.m_QmUiOnlyModified ? s_vModifiedConfigs : s_vCandidateConfigs;

		static CScrollRegion s_ConfigListScrollRegion;
		static CScrollRegion s_ConfigListReadOnlyScrollRegion;
		CScrollRegion &ConfigListScrollRegion = ReadOnly ? s_ConfigListReadOnlyScrollRegion : s_ConfigListScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		SQmScrollRequest ConfigListScrollRequest;
		ConfigListScrollRequest.m_Profile = EQmScrollProfile::SETTINGS_INNER;
		const float ConfigHelpHeight = ConfigMetrics.m_SmallSize;
		const float ResetWidth = TextRender()->TextWidth(FontSize, Localize("Reset")) + MarginSmall * 4.0f;
		ConfigListScrollRequest.m_RowExtent = ResolveSettingsConfigEntryLayout(ListArea, ConfigMetrics, g_Config.m_QmUiCompactList != 0, ResetWidth, ConfigHelpHeight).m_Height;
		CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(QmResolveScrollPolicy(ConfigListScrollRequest, UiScale, 0.0f));
		CPerfTimer ListTimer;
		static float s_PrevConfigsScrollY = 0.0f;
		static float s_PrevConfigsReadOnlyScrollY = 0.0f;
		float &PrevConfigsScrollY = ReadOnly ? s_PrevConfigsReadOnlyScrollY : s_PrevConfigsScrollY;
		SSettingsScrollRegionFrame ScrollFrame = BeginSettingsScrollRegion(ConfigListScrollRegion, &ListArea, ScrollParams, PrevConfigsScrollY);
		ScrollOffset = ScrollFrame.m_BeginOffset;

		ListArea.y += ScrollOffset.y;
		ListArea.VSplitRight(5.0f, &ListArea, nullptr);
		CUIRect Content = ListArea;

		auto SourceName = [](EConfigSource Source) {
			switch(Source)
			{
			case EConfigSource::DDNET: return "DDNet";
			case EConfigSource::TCLIENT: return "TClient";
			case EConfigSource::QM: return "QmClient";
			default: return "Other";
			}
		};

		// 说明只在宽度、字号或语言变化后重新测量，滚动时复用高度。
		static std::unordered_map<const SConfigVariable *, float> s_DescriptionHeights;
		static float s_DescriptionWidth = -1.0f;
		static float s_DescriptionFontSize = -1.0f;
		static uint64_t s_DescriptionLanguageHash = 0;
		const float DescriptionWidth = ResolveSettingsConfigEntryLayout(Content, ConfigMetrics, false, ResetWidth, 0.0f).m_Description.w;
		if(s_DescriptionWidth != DescriptionWidth || s_DescriptionFontSize != ConfigHelpHeight || s_DescriptionLanguageHash != ConfigLanguageHash)
		{
			s_DescriptionHeights.clear();
			s_DescriptionWidth = DescriptionWidth;
			s_DescriptionFontSize = ConfigHelpHeight;
			s_DescriptionLanguageHash = ConfigLanguageHash;
		}
		bool HasCurrentSource = false;
		EConfigSource CurrentSource = EConfigSource::DDNET;
		for(const SConfigVariable *pVar : vpFiltered)
		{
			const EConfigSource Source = GetConfigSource(pVar);
			if(!HasCurrentSource || Source != CurrentSource)
			{
				HasCurrentSource = true;
				CurrentSource = Source;
				CUIRect Header;
				Content.HSplitTop(HeadlineHeight, &Header, &Content);
				if(ConfigListScrollRegion.AddRect(Header))
					Ui()->DoLabel(&Header, SourceName(CurrentSource), HeadlineFontSize, TEXTALIGN_ML);
				Content.HSplitTop(MarginSmall, nullptr, &Content);
			}

			const bool Compact = g_Config.m_QmUiCompactList != 0;
			auto Layout = ResolveSettingsConfigEntryLayout(Content, ConfigMetrics, Compact, ResetWidth, 0.0f);
			float DescriptionHeight = 0.0f;
			if(!Compact)
			{
				auto It = s_DescriptionHeights.find(pVar);
				if(It == s_DescriptionHeights.end())
				{
					const std::string Help = BuildLocalizedConfigHelpText(pVar);
					DescriptionHeight = std::max(ConfigHelpHeight, TextRender()->TextBoundingBox(ConfigHelpHeight, Help.c_str(), -1, std::max(1.0f, Layout.m_Description.w)).m_H);
					s_DescriptionHeights.emplace(pVar, DescriptionHeight);
				}
				else
					DescriptionHeight = It->second;
			}
			Layout = ResolveSettingsConfigEntryLayout(Content, ConfigMetrics, Compact, ResetWidth, DescriptionHeight);
			CUIRect RowItem;
			Content.HSplitTop(Layout.m_Height, &RowItem, &Content);
			Content.HSplitTop(MarginSmall, nullptr, &Content);
			if(!ConfigListScrollRegion.AddRect(RowItem))
				continue;

			const bool Modified = !IsEffectiveDefaultVar(pVar);
			const bool Staged = s_StagedInts.count(pVar) != 0 || s_StagedStrs.count(pVar) != 0 || s_StagedCols.count(pVar) != 0;
			RowItem.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, Staged ? 0.35f : 0.22f), IGraphics::CORNER_ALL, ui_token::radius::BASE);
			if(Modified || Staged)
			{
				CUIRect Marker = RowItem;
				Marker.w = std::min(3.0f * UiScale, Marker.w);
				Marker.Draw(Staged ? color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmUiSelectedColor)) : ColorRGBA(1.0f, 0.8f, 0.0f, 0.65f), IGraphics::CORNER_L, ui_token::radius::BASE);
			}
			CUIRect NameLine = Layout.m_Name;
			CUIRect Controls = Layout.m_Editor;
			CUIRect ResetRect = Layout.m_Reset;
			CUIRect Right{Controls.x, Controls.y, ResetRect.x + ResetRect.w - Controls.x, Controls.h};
			IUiContext TClientConfigTextInputCtx;
			TClientConfigTextInputCtx.m_pUi = Ui();
			TClientConfigTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();
			TClientConfigTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();
			TClientConfigTextInputCtx.m_ScopeHash = MakeUiScopeHash("settings_tclient_config_text_inputs");
			TClientConfigTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
			DoTClientLabel(Ui(), &NameLine, pVar->m_pScriptName, FontSize, TEXTALIGN_ML);
			GameClient()->m_Tooltips.DoToolTipForRect(pVar, &NameLine, pVar->m_pScriptName);
			if(!Compact)
			{
				const std::string Help = BuildLocalizedConfigHelpText(pVar);
				SLabelProperties Props;
				Props.m_MaxWidth = Layout.m_Description.w;
				Props.m_EnableWidthCheck = false;
				Props.m_MinimumFontSize = ConfigHelpHeight;
				Ui()->DoLabel(&Layout.m_Description, Help.c_str(), ConfigHelpHeight, TEXTALIGN_ML, Props);
			}

			static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ResetBtns;
			if(Modified && pVar->m_Type != SConfigVariable::VAR_COLOR)
			{
				CButtonContainer &ResetBtn = s_ResetBtns[pVar];
				if(DoTClientSettingsButton_Menu(&ResetBtn, "tclient-config-reset", Localize("Reset"), 0, &ResetRect))
				{
					if(pVar->m_Type == SConfigVariable::VAR_INT)
					{
						const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
						s_StagedInts[pVar] = {pInt->m_Default};
					}
					else if(pVar->m_Type == SConfigVariable::VAR_STRING)
					{
						const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
						s_StagedStrs[pVar] = {std::string(pStr->m_pDefault)};
					}
				}
			}

			if(pVar->m_Type == SConfigVariable::VAR_INT)
			{
				const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
				// treat 0 1 ints as checkboxes
				Ui()->DoConfigTooltip(pVar, &Right, pInt->m_pVariable);
				GameClient()->m_Tooltips.DoToolTipForRect(pVar, &Right, BuildLocalizedConfigHelpText(pVar).c_str());
				if(pInt->m_Min == 0 && pInt->m_Max == 1)
				{
					const auto StagedInt = s_StagedInts.find(pVar);
					const int Effective = StagedInt != s_StagedInts.end() ? StagedInt->second.m_Value : *pInt->m_pVariable;
					if(DoButton_CheckBox(pVar, "", Effective, &Controls))
					{
						const int NewVal = Effective ? 0 : 1;
						if(NewVal == *pInt->m_pVariable)
							s_StagedInts.erase(pVar);
						else
							s_StagedInts[pVar] = {NewVal};
					}
				}
				else
				{
					SIntState &State = IntInputs[pVar];
					const auto StagedInt = s_StagedInts.find(pVar);
					const int Effective = StagedInt != s_StagedInts.end() ? StagedInt->second.m_Value : *pInt->m_pVariable;
					if(!State.m_Inited)
					{
						State.m_Input.SetInteger(Effective);
						State.m_LastValue = Effective;
						State.m_Inited = true;
					}
					else if(!State.m_Input.IsActive() && State.m_LastValue != Effective)
					{
						State.m_Input.SetInteger(Effective);
						State.m_LastValue = Effective;
					}

					if(ui_widget::InputField(TClientConfigTextInputCtx, &State.m_Input, Controls, nullptr, EditBoxFontSize))
					{
						int NewVal = State.m_Input.GetInteger();
						bool InRange = true;
						if(pInt->m_Min != pInt->m_Max)
						{
							if(NewVal < pInt->m_Min)
								InRange = false;
							if(pInt->m_Max != 0 && NewVal > pInt->m_Max)
								InRange = false;
						}
						if(InRange && NewVal != State.m_LastValue)
						{
							if(NewVal == *pInt->m_pVariable)
								s_StagedInts.erase(pVar);
							else
								s_StagedInts[pVar] = {NewVal};
							State.m_LastValue = NewVal;
						}
					}
				}
			}
			else if(pVar->m_Type == SConfigVariable::VAR_STRING)
			{
				const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
				SStrState &State = StrInputs[pVar];
				Ui()->DoConfigTooltip(pVar, &Right, pStr->m_pStr);
				GameClient()->m_Tooltips.DoToolTipForRect(pVar, &Right, BuildLocalizedConfigHelpText(pVar).c_str());
				const auto StagedStr = s_StagedStrs.find(pVar);
				const char *Effective = StagedStr != s_StagedStrs.end() ? StagedStr->second.m_Value.c_str() : pStr->m_pStr;
				if(!State.m_Inited)
				{
					State.m_vBuffer.resize(pStr->m_MaxSize);
					State.m_Input.SetBuffer(State.m_vBuffer.data(), State.m_vBuffer.size());
					State.m_Input.Set(Effective);
					State.m_Inited = true;
				}
				else if(!State.m_Input.IsActive())
				{
					if(str_comp(State.m_Input.GetString(), Effective) != 0)
						State.m_Input.Set(Effective);
				}

				if(ui_widget::InputField(TClientConfigTextInputCtx, &State.m_Input, Controls, nullptr, EditBoxFontSize))
				{
					const char *NewVal = State.m_Input.GetString();
					if(str_comp(NewVal, pStr->m_pStr) == 0)
						s_StagedStrs.erase(pVar);
					else
						s_StagedStrs[pVar] = {std::string(NewVal)};
				}
			}
			else if(pVar->m_Type == SConfigVariable::VAR_COLOR)
			{
				const SColorConfigVariable *pCol = static_cast<const SColorConfigVariable *>(pVar);
				Ui()->DoConfigTooltip(pVar, &Right, pCol->m_pVariable);
				GameClient()->m_Tooltips.DoToolTipForRect(pVar, &Right, BuildLocalizedConfigHelpText(pVar).c_str());
				const ColorRGBA DefaultColor = color_cast<ColorRGBA>(ColorHSLA(pCol->m_Default, true).UnclampLighting(pCol->m_DarkestLighting));
				static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ColorResetIds;
				CButtonContainer &ResetId = s_ColorResetIds[pVar];

				SColState &ColState = ColInputs[pVar];
				const auto StagedCol = s_StagedCols.find(pVar);
				unsigned Effective = StagedCol != s_StagedCols.end() ? StagedCol->second.m_Value : *pCol->m_pVariable;
				if(!ColState.m_Inited)
				{
					ColState.m_Working = Effective;
					ColState.m_LastValue = Effective;
					ColState.m_Inited = true;
				}
				else
				{
					const bool EditingThis = Ui()->IsPopupOpen(&m_ColorPickerPopupContext) && m_ColorPickerPopupContext.m_pHslaColor == &ColState.m_Working;
					if(!EditingThis && ColState.m_Working != Effective)
					{
						ColState.m_Working = Effective;
						ColState.m_LastValue = Effective;
					}
				}

				CUIRect ColorRect = Controls;
				ColorRect.w = std::min(ColorRect.w, ColorRect.h);
				DoButton_ColorPicker(&ColorRect, &ColState.m_Working, pCol->m_Alpha);
				if(DoTClientSettingsButton_Menu(&ResetId, "tclient-config-reset", Localize("Reset"), 0, &ResetRect))
					ColState.m_Working = color_cast<ColorHSLA>(DefaultColor).Pack(pCol->m_Alpha);
				if(ColState.m_Working != Effective)
				{
					if(ColState.m_Working == *pCol->m_pVariable)
						s_StagedCols.erase(pVar);
					else
						s_StagedCols[pVar] = {ColState.m_Working};
					ColState.m_LastValue = ColState.m_Working;
				}
			}
		}

		CUIRect EndPad{Content.x, Content.y, Content.w, 5.0f};
		FinishSettingsScrollRegion(ConfigListScrollRegion, ScrollFrame, &EndPad);
		PrevConfigsScrollY = ScrollFrame.m_FinalOffsetY;
		char aExtra[96];
		str_format(aExtra, sizeof(aExtra), "filtered=%d scroll_y=%.1f", (int)vpFiltered.size(), ScrollFrame.m_FinalOffsetY);
		LogTClientPerfStageEx("tclient_configs", "list", ETClientSettingsPerfStage::STATIC_LAYER, ListTimer.ElapsedMs(), false, aExtra);
	};

	const auto RunConfigLayout = [&](CUIRect Content, bool Draw) {
		const float StartY = Content.y;
		RenderFilters(Content, Draw);
		const float ListHeight = std::max(LineSize * 4.0f, std::min(Content.w * 0.42f, 260.0f * UiScale));
		CUIRect List;
		Content.HSplitTop(ListHeight, &List, &Content);
		Content.HSplitTop(ConfigMetrics.m_SectionGap, nullptr, &Content);
		CUIRect ChangesHeading;
		const float ChangesHeadingHeight = std::max(HeadlineHeight, TextRender()->TextBoundingBox(HeadlineFontSize, Localize("Config Changes"), -1, std::max(1.0f, Content.w)).m_H);
		Content.HSplitTop(ChangesHeadingHeight, &ChangesHeading, &Content);
		Content.HSplitTop(MarginSmall, nullptr, &Content);
		if(Draw)
		{
			RenderList(List);
			DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, "tclient-config-changes-heading", &ChangesHeading, Localize("Config Changes"), HeadlineFontSize, TEXTALIGN_ML);
		}
		ChangesCount = s_StagedInts.size() + s_StagedStrs.size() + s_StagedCols.size();
		RenderActions(Content, Draw);
		return Content.y - StartY;
	};
	// 暂存数量会改变底部计数的宽度；页面和搜索卡片使用相同的重测条件。
	const uint64_t CardRevision = (static_cast<uint64_t>(ChangesCount) << 1) | static_cast<uint64_t>(g_Config.m_QmUiCompactList != 0);
	if(Pass == qm_card_catalog::ETClientCardPass::REVISION)
		return {0.0f, CardRevision};
	if(str_comp(pStableId, "deck:tclient-configs-actions") == 0)
		return {RunConfigLayout(Content, Pass == qm_card_catalog::ETClientCardPass::RENDER), CardRevision};
	return {0.0f, CardRevision};
}
