/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>
#include <base/math.h>
#include <base/perf_timer.h>
#include <base/system.h>

#include <engine/client/backend/graphics_backend_contract.h>
#include <engine/external/tinyexpr.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <generated/protocol.h>

#include <game/client/QmUi/QmAnimResolve.h>
#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/QmUiPerf.h>
#include <game/client/QmUi/SecondaryPanel.h>
#include <game/client/QmUi/SettingsCard.h>
#include <game/client/QmUi/SettingsIconOptions.h>
#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/UiContext.h>
#include <game/client/QmUi/UiForms.h>
#include <game/client/QmUi/UiNavigation.h>
#include <game/client/QmUi/UiSurface.h>
#include <game/client/QmUi/UiTokens.h>
#include <game/client/QmUi/cards/QmCardCatalog.h>
#include <game/client/QmUi/cards/QmCardCatalogTeeMetrics.h>
#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/message_gradient.h>
#include <game/client/components/qmclient/modes.h>
#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/components/qmclient/settings_resource_preview.h>
#include <game/client/components/qmclient/tee_color_code.h>
#include <game/client/components/qmclient/tee_hue_cycle.h>
#include <game/client/components/qmclient/tee_skin_apply.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/qm_icon.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace FontIcons;
using namespace std::chrono_literals;

namespace
{
	void LogPerfStage(IClient *pClient, const char *pStage, double DurationMs, bool Force = false, const char *pExtra = nullptr)
	{
		QmPerfLogStage("perf/menu", pStage, DurationMs, Force, pClient, nullptr, nullptr, pExtra);
	}
}

namespace
{
	struct SAudioPackEntry
	{
		char m_aName[64];
		int m_FileCount;
	};

	struct SAudioPackScanUser
	{
		IStorage *m_pStorage;
		std::vector<SAudioPackEntry> *m_pPacks;
	};
} // namespace

static int AudioPackFileScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(IsDir || pName[0] == '.')
		return 0;

	if(str_endswith(pName, ".wv") || str_endswith(pName, ".opus"))
	{
		int *pCount = static_cast<int *>(pUser);
		(*pCount)++;
	}

	return 0;
}

static int AudioPackScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(!IsDir || pName[0] == '.' || str_comp(pName, "default") == 0)
		return 0;

	auto *pData = static_cast<SAudioPackScanUser *>(pUser);
	SAudioPackEntry Entry{};
	str_copy(Entry.m_aName, pName, sizeof(Entry.m_aName));

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "audio/%s", pName);
	pData->m_pStorage->ListDirectory(IStorage::TYPE_ALL, aPath, AudioPackFileScan, &Entry.m_FileCount);

	if(Entry.m_FileCount == 0)
	{
		str_format(aPath, sizeof(aPath), "audio/%s/audio", pName);
		pData->m_pStorage->ListDirectory(IStorage::TYPE_ALL, aPath, AudioPackFileScan, &Entry.m_FileCount);
	}

	pData->m_pPacks->push_back(Entry);
	return 0;
}

static void RefreshAudioPacks(IStorage *pStorage, std::vector<SAudioPackEntry> &vPacks)
{
	vPacks.clear();

	SAudioPackEntry Default{};
	str_copy(Default.m_aName, "default", sizeof(Default.m_aName));
	pStorage->ListDirectory(IStorage::TYPE_ALL, "audio", AudioPackFileScan, &Default.m_FileCount);
	vPacks.push_back(Default);

	SAudioPackScanUser User{pStorage, &vPacks};
	pStorage->ListDirectory(IStorage::TYPE_ALL, "audio", AudioPackScan, &User);

	if(vPacks.size() > 1)
	{
		std::sort(vPacks.begin() + 1, vPacks.end(), [](const SAudioPackEntry &A, const SAudioPackEntry &B) {
			return str_comp(A.m_aName, B.m_aName) < 0;
		});
	}
}

static std::vector<SAudioPackEntry> gs_vAudioPacks;
static bool gs_AudioPacksInit = false;

static void RefreshSharedAudioPacks(IStorage *pStorage)
{
	RefreshAudioPacks(pStorage, gs_vAudioPacks);
	gs_AudioPacksInit = true;
}

static void EnsureSharedAudioPacks(IStorage *pStorage)
{
	if(!gs_AudioPacksInit)
		RefreshSharedAudioPacks(pStorage);
}

static int FindAudioPackIndexByName(const std::vector<SAudioPackEntry> &vPacks, const char *pPackName)
{
	for(size_t i = 0; i < vPacks.size(); ++i)
	{
		if(str_comp(vPacks[i].m_aName, pPackName) == 0)
			return (int)i;
	}
	return -1;
}

void CMenus::RefreshAudioPackList()
{
	RefreshSharedAudioPacks(Storage());
}

uint64_t CMenus::BuildSoundSettingsCards(const qm_card_catalog::SQmCardBuildContext &Ctx, std::vector<SSettingsCardDefinition> *pCards)
{
	const CUIRect MainView = Ctx.m_Page.m_ContentViewport;

	static bool s_SndPackInit = false;
	static char s_aSndPack[sizeof(g_Config.m_SndPack)] = "";

	if(!s_SndPackInit)
	{
		str_copy(s_aSndPack, g_Config.m_SndPack, sizeof(s_aSndPack));
		s_SndPackInit = true;
	}

	const SSettingsContentMetrics SoundMetrics = Ctx.m_Metrics;
	const float UiScale = SoundMetrics.m_UiScale;
	const float BodySize = SoundMetrics.m_BodySize;
	const float LineHeight = SoundMetrics.m_LineHeight;
	const float LineSpacing = SoundMetrics.m_LineSpacing;
	const IUiContext SoundCardCtx = Ctx.m_UiContext;
	const auto DoSoundNumericField = [this, SoundCardCtx, BodySize](const char *pTextId, const void *pId, int *pOption, const CUIRect &Rect, const char *pLabel) {
		ui_widget::SNumericFieldOptions Options;
		Options.m_pLabel = pLabel;
		Options.m_pSuffix = "%";
		Options.m_pScale = &CUi::ms_LogarithmicScrollbarScale;
		Options.m_FontSize = BodySize;
		Options.m_LabelAlign = TEXTALIGN_ML;
		if(PrepareSettingsNumericFieldLabel(SETTINGS_SOUND, -1, -1, pTextId, Rect, pLabel, 0u, Options))
			return false;
		return ui_widget::NumericField(SoundCardCtx, GetSettingsNumericFieldState(pId), pId, pOption, 0, 100, Rect, Options);
	};
	const qm_card_registry::SCardDefault *pToggleDefault = qm_card_registry::FindByStableId("deck:sound-toggle");
	const qm_card_registry::SCardDefault *pVolumeDefault = qm_card_registry::FindByStableId("deck:sound-volume");
	const qm_card_registry::SCardDefault *pAudioPackDefault = qm_card_registry::FindByStableId("deck:sound-audio-pack");
	dbg_assert(pToggleDefault != nullptr && pVolumeDefault != nullptr && pAudioPackDefault != nullptr, "sound settings cards must be registered");
	if(pToggleDefault == nullptr || pVolumeDefault == nullptr || pAudioPackDefault == nullptr)
		return 0;

	const float CardChromeHeight = BuildSettingsCardFrame({0.0f, 0.0f, 1.0f, 0.0f}, {nullptr, nullptr, "subtitle"}, 0.0f, UiScale).m_Rect.h;
	const float ToggleChromeHeight = CardChromeHeight;
	const float VolumeChromeHeight = CardChromeHeight;
	const float AudioPackChromeHeight = CardChromeHeight;
	EnsureSharedAudioPacks(Storage());
	const int ToggleRowCount = g_Config.m_SndEnable ? 10 : 1;
	const float SoundToggleCardHeight = ToggleChromeHeight + LineHeight * ToggleRowCount + LineSpacing * maximum(0, ToggleRowCount - 1);
	const float SoundVolumeCardHeight = VolumeChromeHeight + LineHeight * 5.0f + LineSpacing * 4.0f;
	const int AudioPackCount = (int)gs_vAudioPacks.size();
	const SSettingsListCardGeometry SoundAudioPackGeometry = ResolveSettingsSoundAudioPackGeometry(AudioPackCount, SoundMetrics);
	const float SoundAudioPackContentHeight = SoundAudioPackGeometry.m_ContentHeight;
	const float SoundAudioPackCardHeight = AudioPackChromeHeight + SoundAudioPackContentHeight;
	const bool RenderOnly = Ctx.m_ReadOnly;
	const auto BuildDefinitions = [this, pToggleDefault, pVolumeDefault, pAudioPackDefault, SoundToggleCardHeight, ToggleChromeHeight, SoundVolumeCardHeight, VolumeChromeHeight, SoundAudioPackCardHeight, AudioPackChromeHeight, SoundMetrics, LineHeight, LineSpacing, BodySize, DoSoundNumericField](std::vector<SSettingsCardDefinition> &vCards) {
		vCards.reserve(3);
		const SSettingsCardSpec ToggleSpec{pToggleDefault->m_pStableId, Localize(pToggleDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pToggleDefault)};
		const SSettingsCardSpec VolumeSpec{pVolumeDefault->m_pStableId, Localize(pVolumeDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pVolumeDefault)};
		const SSettingsCardSpec AudioPackSpec{pAudioPackDefault->m_pStableId, Localize(pAudioPackDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pAudioPackDefault)};
		const auto AddCard = [&vCards](const SSettingsCardSpec &Spec, float TotalHeight, float ChromeHeight, FSettingsCardRender Render, std::function<bool()> IsVisible = {}, bool VisibilityController = false, FSettingsCardPreLayoutInput PreLayoutInput = {}, uint64_t MeasureRevision = 0) {
			SSettingsCardDefinition Definition;
			Definition.m_Spec = Spec;
			Definition.m_Measure = [TotalHeight, ChromeHeight](float) {
				return maximum(0.0f, TotalHeight - ChromeHeight);
			};
			Definition.m_Render = std::move(Render);
			Definition.m_PreLayoutInput = std::move(PreLayoutInput);
			Definition.m_IsVisible = std::move(IsVisible);
			Definition.m_VisibilityController = VisibilityController;
			Definition.m_MeasureRevision = MeasureRevision;
			vCards.push_back(std::move(Definition));
		};
		const auto ProcessSoundToggleInput = [this, LineHeight](CUIRect ContentRect) {
			if(m_MenuTextPlanCollecting)
				return false;
			CUIRect MainView = ContentRect;
			CUIRect Button;
			MainView.HSplitTop(LineHeight, &Button, &MainView);
			if(Ui()->DoButtonLogic(&g_Config.m_SndEnable, 0, &Button, BUTTONFLAG_LEFT))
			{
				g_Config.m_SndEnable ^= 1;
				UpdateMusicState();
				return true;
			}
			return false;
		};
		AddCard(ToggleSpec, SoundToggleCardHeight, ToggleChromeHeight, [this, LineHeight, LineSpacing](CUIRect ContentRect) {
		CUIRect MainView = ContentRect;
		CUIRect Button;

		MainView.HSplitTop(LineHeight, &Button, &MainView);
		DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, -1, &g_Config.m_SndEnable, "Use sounds", Localize("Use sounds"), g_Config.m_SndEnable, &Button, SLabelProperties{}, false);

		m_NeedRestartSound = g_Config.m_SndEnable && !Sound()->IsSoundEnabled();
		if(!g_Config.m_SndEnable)
		{
			const bool PackChanged = str_comp(g_Config.m_SndPack, s_aSndPack) != 0;
			m_NeedRestartSound = m_NeedRestartSound || PackChanged;
			return;
		}

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndMusic, "Play background music", Localize("Play background music"), g_Config.m_SndMusic, &Button))
		{
			g_Config.m_SndMusic ^= 1;
			UpdateMusicState();
		}

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndNonactiveMute, "Mute when not active", Localize("Mute when not active"), g_Config.m_SndNonactiveMute, &Button))
			g_Config.m_SndNonactiveMute ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndGame, "Enable game sounds", Localize("Enable game sounds"), g_Config.m_SndGame, &Button))
			g_Config.m_SndGame ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndGun, "Enable gun sound", Localize("Enable gun sound"), g_Config.m_SndGun, &Button))
			g_Config.m_SndGun ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndLongPain, "Enable shout when holding fire while in water", Localize("Enable shout when holding fire while in water"), g_Config.m_SndLongPain, &Button))
			g_Config.m_SndLongPain ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndServerMessage, "Enable server message sound", Localize("Enable server message sound"), g_Config.m_SndServerMessage, &Button))
			g_Config.m_SndServerMessage ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndChat, "Enable regular chat sound", Localize("Enable regular chat sound"), g_Config.m_SndChat, &Button))
			g_Config.m_SndChat ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndTeamChat, "Enable team chat sound", Localize("Enable team chat sound"), g_Config.m_SndTeamChat, &Button))
			g_Config.m_SndTeamChat ^= 1;

		MainView.HSplitTop(LineSpacing, nullptr, &MainView);
		MainView.HSplitTop(LineHeight, &Button, &MainView);
		if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndHighlight, "Enable highlighted chat sound", Localize("Enable highlighted chat sound"), g_Config.m_SndHighlight, &Button))
			g_Config.m_SndHighlight ^= 1; }, {}, true, ProcessSoundToggleInput, g_Config.m_SndEnable);
		vCards.back().m_Measure = [LineHeight, LineSpacing](float) {
			const int RowCount = g_Config.m_SndEnable ? 10 : 1;
			return LineHeight * RowCount + LineSpacing * maximum(0, RowCount - 1);
		};

		AddCard(VolumeSpec, SoundVolumeCardHeight, VolumeChromeHeight, [DoSoundNumericField, LineHeight, LineSpacing](CUIRect ContentRect) {
			CUIRect MainView = ContentRect;
			CUIRect VolumeButton;
			MainView.HSplitTop(LineHeight, &VolumeButton, &MainView);
			DoSoundNumericField("sound-volume", &g_Config.m_SndVolume, &g_Config.m_SndVolume, VolumeButton, Localize("Sound volume"));
			MainView.HSplitTop(LineSpacing, nullptr, &MainView);
			MainView.HSplitTop(LineHeight, &VolumeButton, &MainView);
			DoSoundNumericField("sound-game-volume", &g_Config.m_SndGameVolume, &g_Config.m_SndGameVolume, VolumeButton, Localize("Game sound volume"));
			MainView.HSplitTop(LineSpacing, nullptr, &MainView);
			MainView.HSplitTop(LineHeight, &VolumeButton, &MainView);
			DoSoundNumericField("sound-chat-volume", &g_Config.m_SndChatVolume, &g_Config.m_SndChatVolume, VolumeButton, Localize("Chat sound volume"));
			MainView.HSplitTop(LineSpacing, nullptr, &MainView);
			MainView.HSplitTop(LineHeight, &VolumeButton, &MainView);
			DoSoundNumericField("sound-map-volume", &g_Config.m_SndMapVolume, &g_Config.m_SndMapVolume, VolumeButton, Localize("Map sound volume"));
			MainView.HSplitTop(LineSpacing, nullptr, &MainView);
			MainView.HSplitTop(LineHeight, &VolumeButton, &MainView);
			DoSoundNumericField("sound-background-music-volume", &g_Config.m_SndBackgroundMusicVolume, &g_Config.m_SndBackgroundMusicVolume, VolumeButton, Localize("Background music volume")); }, []() { return g_Config.m_SndEnable != 0; });
		AddCard(AudioPackSpec, SoundAudioPackCardHeight, AudioPackChromeHeight, [this, SoundMetrics, LineHeight, LineSpacing, BodySize](CUIRect ContentRect) {
			CUIRect AudioPackView = ContentRect;
			static CButtonContainer s_AudioPackRefreshButton;
			static CButtonContainer s_AudioPackEditorButton;
			static CButtonContainer s_AudioPackDirectoryButton;
			static CListBox s_AudioPackListBox;
			EnsureSharedAudioPacks(Storage());

			auto RefreshAudioPackState = [&]() {
				RefreshSharedAudioPacks(Storage());
				if(g_Config.m_SndPack[0] == '\0')
					str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));
				if(m_AudioPackEditorState.m_PackNameInput.IsEmpty())
					m_AudioPackEditorState.m_PackNameInput.Set(g_Config.m_SndPack);
			};

			if(g_Config.m_SndPack[0] == '\0')
				str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));

			auto FindSelectedPackIndex = [&]() {
				int Result = FindAudioPackIndexByName(gs_vAudioPacks, g_Config.m_SndPack);
				if(Result < 0)
				{
					RefreshSharedAudioPacks(Storage());
					Result = FindAudioPackIndexByName(gs_vAudioPacks, g_Config.m_SndPack);
				}
				if(Result < 0)
				{
					str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));
					Result = 0;
				}
				return Result;
			};
			int SelectedPack = FindSelectedPackIndex();

			CUIRect AudioPackContent;
			AudioPackView.Margin(2.0f, &AudioPackContent);
			CUIRect HeaderRow, ListRow;
			AudioPackContent.HSplitTop(LineHeight, &HeaderRow, &AudioPackContent);
			AudioPackContent.HSplitTop(LineSpacing, nullptr, &AudioPackContent);
			ListRow = AudioPackContent;

			const float RefreshButtonW = LineHeight + LineSpacing;
			const float EditButtonW = minimum(168.0f, maximum(114.0f, TextRender()->TextWidth(BodySize, Localize("Edit audio pack"), -1, -1.0f) + 22.0f));
			const float DirectoryButtonW = minimum(176.0f, maximum(122.0f, TextRender()->TextWidth(BodySize, Localize("Audio pack directory"), -1, -1.0f) + 22.0f));
			CUIRect EditButton;
			CUIRect DirectoryButton;
			CUIRect RefreshButton;
			HeaderRow.VSplitRight(RefreshButtonW, &HeaderRow, &RefreshButton);
			RefreshButton.VMargin(2.0f, &RefreshButton);
			if(Ui()->DoButton_QmIcon(&s_AudioPackRefreshButton, EQmIcon::ARROW_ROTATE_RIGHT, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &RefreshButton, BUTTONFLAG_LEFT))
			{
				RefreshAudioPackState();
				SelectedPack = FindSelectedPackIndex();
			}
			HeaderRow.VSplitLeft(EditButtonW, &EditButton, &HeaderRow);
			EditButton.VMargin(2.0f, &EditButton);
			if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorButton, "sound-edit-audio-pack", Localize("Edit audio pack"), 0, &EditButton))
				AudioPackEditorOpen(g_Config.m_SndPack);
			HeaderRow.VSplitLeft(6.0f, nullptr, &HeaderRow);
			HeaderRow.VSplitLeft(DirectoryButtonW, &DirectoryButton, &HeaderRow);
			DirectoryButton.VMargin(2.0f, &DirectoryButton);
			if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackDirectoryButton, "sound-audio-pack-directory", Localize("Audio pack directory"), 0, &DirectoryButton))
			{
				char aBuf[IO_MAX_PATH_LENGTH];
				Storage()->GetCompletePath(IStorage::TYPE_SAVE, "audio", aBuf, sizeof(aBuf));
				Client()->ViewFile(aBuf);
			}

			DrawRoundedSurface(Ui(), ListRow, ColorRGBA(0.0f, 0.0f, 0.0f, 0.12f), ColorRGBA(), 6.0f);
			ListRow.Margin(6.0f, &ListRow);

			const int OldSelectedPack = SelectedPack;
			s_AudioPackListBox.SetScrollProfile(EQmScrollProfile::SETTINGS_INNER);
			s_AudioPackListBox.SetWheelOwnerPriority(EUiWheelOwnerPriority::COMPOSITE_CONTROL);
			s_AudioPackListBox.SetItemColors(ui_token::color::LIST_ITEM_SELECTED, ui_token::color::LIST_ITEM_SELECTED, ui_token::color::LIST_ITEM_HOVER);
			s_AudioPackListBox.DoStart(LineHeight + LineSpacing, gs_vAudioPacks.size(), 1, 4, SelectedPack, &ListRow, false);

			// 数量徽章在行内居中，使用紧凑尺寸并保留上下间隙。
			const float BadgeFontSize = SoundMetrics.m_SmallSize;
			const float BadgeWidth = 28.0f * SoundMetrics.m_UiScale;
			const float BadgeHeight = SoundMetrics.m_BadgeHeight;

			for(size_t i = 0; i < gs_vAudioPacks.size(); ++i)
			{
				const SAudioPackEntry &Entry = gs_vAudioPacks[i];
				const CListboxItem Item = s_AudioPackListBox.DoNextItem(&Entry, SelectedPack == (int)i);
				if(!Item.m_Visible)
					continue;

				char aLabel[128];
				if(str_comp(Entry.m_aName, "default") == 0)
				{
					str_copy(aLabel, Localize("Default"), sizeof(aLabel));
				}
				else
				{
					str_copy(aLabel, Entry.m_aName, sizeof(aLabel));
				}

				CUIRect NameRect, BadgeRect;
				Item.m_Rect.VSplitRight(BadgeWidth, &NameRect, &BadgeRect);
				NameRect.VMargin(6.0f, &NameRect);
				BadgeRect.HMargin((BadgeRect.h - BadgeHeight) * 0.5f, &BadgeRect);

				char aBadge[32];
				str_format(aBadge, sizeof(aBadge), "%d", Entry.m_FileCount);
				DrawRoundedSurface(Ui(), BadgeRect, SelectedPack == (int)i ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), ColorRGBA(), ui_token::radius::TIGHT);

				Ui()->DoLabel(&NameRect, aLabel, BodySize, TEXTALIGN_ML);
				Ui()->DoLabel(&BadgeRect, aBadge, BadgeFontSize, TEXTALIGN_MC);
			}

			SelectedPack = s_AudioPackListBox.DoEnd();
			if(SelectedPack != OldSelectedPack && SelectedPack >= 0 && SelectedPack < (int)gs_vAudioPacks.size())
			{
				str_copy(g_Config.m_SndPack, gs_vAudioPacks[SelectedPack].m_aName, sizeof(g_Config.m_SndPack));
				if(GameClient()->m_Sounds.Reload())
				{
					str_copy(s_aSndPack, g_Config.m_SndPack, sizeof(s_aSndPack));
					UpdateMusicState();
				}
				if(!m_AudioPackEditorState.m_PackNameInput.IsActive())
					m_AudioPackEditorState.m_PackNameInput.Set(g_Config.m_SndPack);
			} }, []() { return g_Config.m_SndEnable != 0; });
	};
	const uint64_t SoundLayoutRevision = ResolveSettingsSoundLayoutRevision(RenderOnly, g_Config.m_SndEnable != 0, AudioPackCount);
	if(pCards != nullptr)
	{
		BuildDefinitions(*pCards);
	}
	m_NeedRestartSound = m_NeedRestartSound || str_comp(g_Config.m_SndPack, s_aSndPack) != 0;
	return SoundLayoutRevision;
}
