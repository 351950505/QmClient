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
#include <game/client/QmUi/SettingsCardContentBridge.h>
#include <game/client/QmUi/SettingsIconOptions.h>
#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/UiButtons.h>
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

void CMenus::AudioPackEditorOpen(const char *pPackName)
{
	AudioPackEditorStopPreview();
	g_Config.m_UiSettingsPage = SETTINGS_SOUND;
	m_AudioPackEditorState.m_Open = true;
	g_Config.m_UiSettingsPage = SETTINGS_SOUND;
	m_AudioPackEditorState.m_Initialized = false;
	m_AudioPackEditorState.m_SelectedSlotIndex = 0;
	m_AudioPackEditorState.m_SelectedCandidateIndex = -1;
	m_AudioPackEditorState.m_StatusIsError = false;
	m_AudioPackEditorState.m_aStatusMessage[0] = '\0';
	m_AudioPackEditorState.m_FilterInput.Clear();
	m_AudioPackEditorState.m_CandidateFilterInput.Clear();
	m_AudioPackEditorState.m_SourcePathInput.Clear();
	if(pPackName != nullptr && pPackName[0] != '\0')
		m_AudioPackEditorState.m_PackNameInput.Set(pPackName);
	else
		m_AudioPackEditorState.m_PackNameInput.Set("default");
}

void CMenus::AudioPackEditorClose()
{
	AudioPackEditorStopPreview();
	m_AudioPackEditorState.m_Open = false;
	m_AudioPackEditorState.m_Initialized = false;
	m_AudioPackEditorState.m_SelectedCandidateIndex = -1;
	m_AudioPackEditorState.m_vCandidateEntries.clear();
}

void CMenus::AudioPackEditorSetStatus(const char *pMessage, bool IsError)
{
	m_AudioPackEditorState.m_StatusIsError = IsError;
	str_copy(m_AudioPackEditorState.m_aStatusMessage, pMessage != nullptr ? pMessage : "", sizeof(m_AudioPackEditorState.m_aStatusMessage));
}

namespace
{
	struct SAudioPackCandidateScanContext
	{
		IStorage *m_pStorage = nullptr;
		std::set<std::string> *m_pEntries = nullptr;
		char m_aScanRoot[IO_MAX_PATH_LENGTH] = "";
		char m_aOutputPrefix[IO_MAX_PATH_LENGTH] = "";
		char m_aRelativePath[IO_MAX_PATH_LENGTH] = "";
	};

}

namespace
{

	static int AudioPackCandidateScanCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
	{
		(void)StorageType;

		auto *pContext = static_cast<SAudioPackCandidateScanContext *>(pUser);
		if(!str_comp(pInfo->m_pName, ".") || !str_comp(pInfo->m_pName, ".."))
			return 0;

		char aRelativePath[IO_MAX_PATH_LENGTH];
		if(pContext->m_aRelativePath[0] != '\0')
			str_format(aRelativePath, sizeof(aRelativePath), "%s/%s", pContext->m_aRelativePath, pInfo->m_pName);
		else
			str_copy(aRelativePath, pInfo->m_pName);

		char aScanPath[IO_MAX_PATH_LENGTH];
		str_format(aScanPath, sizeof(aScanPath), "%s/%s", pContext->m_aScanRoot, aRelativePath);

		if(IsDir)
		{
			if(pInfo->m_pName[0] == '.')
				return 0;

			SAudioPackCandidateScanContext NextContext = *pContext;
			str_copy(NextContext.m_aRelativePath, aRelativePath, sizeof(NextContext.m_aRelativePath));
			pContext->m_pStorage->ListDirectoryInfo(IStorage::TYPE_ALL, aScanPath, AudioPackCandidateScanCallback, &NextContext);
			return 0;
		}

		std::string CandidatePath;
		if(CMenus::TryBuildAudioPackCandidatePathFromScan(pContext->m_aOutputPrefix, aRelativePath, CandidatePath))
			pContext->m_pEntries->insert(std::move(CandidatePath));

		return 0;
	}

	static const char *ResolveAudioPackEditorPackName(const CLineInputBuffered<64> &PackNameInput, const char *pFallbackPackName)
	{
		if(PackNameInput.GetString()[0] != '\0')
			return PackNameInput.GetString();
		return pFallbackPackName != nullptr ? pFallbackPackName : "";
	}

	static void ResolveAudioPackEditorCurrentFilePath(IStorage *pStorage, const char *pPackName, const CMenus::SAudioPackSlot &Slot, char *pOut, int OutSize)
	{
		pOut[0] = '\0';

		char aDirectPath[IO_MAX_PATH_LENGTH];
		char aLegacyPath[IO_MAX_PATH_LENGTH];
		char aBuiltinPath[IO_MAX_PATH_LENGTH];

		str_copy(aDirectPath, CMenus::BuildAudioPackExportPath(pPackName, Slot.m_pRelativePath).c_str(), sizeof(aDirectPath));
		str_format(aLegacyPath, sizeof(aLegacyPath), "audio/%s/audio/%s", pPackName, Slot.m_pRelativePath);
		str_copy(aBuiltinPath, CMenus::BuildAudioPackBuiltinCandidatePath(Slot.m_pRelativePath).c_str(), sizeof(aBuiltinPath));

		if(pStorage->FileExists(aDirectPath, IStorage::TYPE_ALL))
			str_copy(pOut, aDirectPath, OutSize);
		else if(pStorage->FileExists(aLegacyPath, IStorage::TYPE_ALL))
			str_copy(pOut, aLegacyPath, OutSize);
		else if(pStorage->FileExists(aBuiltinPath, IStorage::TYPE_ALL))
			str_copy(pOut, aBuiltinPath, OutSize);
	}

}

void CMenus::AudioPackEditorRefreshCandidates()
{
	std::set<std::string> vCandidatePaths;
	for(const auto &ScanRoot : BuildAudioPackCandidateScanRoots())
	{
		if(!Storage()->FolderExists(ScanRoot.m_pScanRoot, IStorage::TYPE_ALL))
			continue;

		SAudioPackCandidateScanContext Context;
		Context.m_pStorage = Storage();
		Context.m_pEntries = &vCandidatePaths;
		str_copy(Context.m_aScanRoot, ScanRoot.m_pScanRoot, sizeof(Context.m_aScanRoot));
		str_copy(Context.m_aOutputPrefix, ScanRoot.m_pOutputPrefix, sizeof(Context.m_aOutputPrefix));
		Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, ScanRoot.m_pScanRoot, AudioPackCandidateScanCallback, &Context);
	}

	std::vector<std::string> vPaths(vCandidatePaths.begin(), vCandidatePaths.end());

	char aCurrentPath[IO_MAX_PATH_LENGTH] = "";
	const auto vSlots = BuildAudioPackSlots();
	if(!vSlots.empty())
	{
		m_AudioPackEditorState.m_SelectedSlotIndex = std::clamp(m_AudioPackEditorState.m_SelectedSlotIndex, 0, (int)vSlots.size() - 1);
		ResolveAudioPackEditorCurrentFilePath(Storage(), ResolveAudioPackEditorPackName(m_AudioPackEditorState.m_PackNameInput, g_Config.m_SndPack), vSlots[m_AudioPackEditorState.m_SelectedSlotIndex], aCurrentPath, sizeof(aCurrentPath));
	}

	std::string SelectedPath;
	if(m_AudioPackEditorState.m_SelectedCandidateIndex >= 0 && m_AudioPackEditorState.m_SelectedCandidateIndex < (int)m_AudioPackEditorState.m_vCandidateEntries.size())
		SelectedPath = m_AudioPackEditorState.m_vCandidateEntries[m_AudioPackEditorState.m_SelectedCandidateIndex].m_Path;

	m_AudioPackEditorState.m_vCandidateEntries = BuildAudioPackCandidateEntries(vPaths, ResolveAudioPackEditorPackName(m_AudioPackEditorState.m_PackNameInput, g_Config.m_SndPack), aCurrentPath);

	int SelectedIndex = FindAudioPackCandidateEntryIndex(m_AudioPackEditorState.m_vCandidateEntries, aCurrentPath);
	if(SelectedIndex < 0 && !SelectedPath.empty())
		SelectedIndex = FindAudioPackCandidateEntryIndex(m_AudioPackEditorState.m_vCandidateEntries, SelectedPath.c_str());
	if(SelectedIndex < 0 && !m_AudioPackEditorState.m_vCandidateEntries.empty())
		SelectedIndex = 0;
	m_AudioPackEditorState.m_SelectedCandidateIndex = SelectedIndex;
}

void CMenus::AudioPackEditorStopPreview()
{
	if(m_AudioPackEditorState.m_PreviewSampleId >= 0)
	{
		Sound()->Stop(m_AudioPackEditorState.m_PreviewSampleId);
		Sound()->UnloadSample(m_AudioPackEditorState.m_PreviewSampleId);
		m_AudioPackEditorState.m_PreviewSampleId = -1;
	}
}

bool CMenus::AudioPackEditorPlayPreview(const char *pFilename, int StorageType)
{
	if(pFilename == nullptr || pFilename[0] == '\0')
		return false;

	AudioPackEditorStopPreview();

	const int SampleId = Sound()->LoadWV(pFilename, StorageType);
	if(SampleId < 0)
		return false;

	m_AudioPackEditorState.m_PreviewSampleId = SampleId;
	GameClient()->m_Sounds.PlaySample(CSounds::CHN_GUI, SampleId, ISound::FLAG_PREVIEW, 1.0f);
	return true;
}

bool CMenus::AudioPackEditorEnsureStorageDirectories(const char *pStoragePath)
{
	if(pStoragePath == nullptr || pStoragePath[0] == '\0')
		return false;

	std::string CurrentDirectory;
	for(const char *pCursor = pStoragePath; *pCursor != '\0'; ++pCursor)
	{
		if(*pCursor == '/')
		{
			if(!CurrentDirectory.empty() && !Storage()->FolderExists(CurrentDirectory.c_str(), IStorage::TYPE_SAVE) &&
				!Storage()->CreateFolder(CurrentDirectory.c_str(), IStorage::TYPE_SAVE))
			{
				return false;
			}
		}
		else
		{
			CurrentDirectory.push_back(*pCursor);
		}
	}

	return true;
}

bool CMenus::AudioPackEditorCopyFileToStorage(const char *pSourcePath, int SourceStorageType, const char *pStoragePath)
{
	if(pSourcePath == nullptr || pSourcePath[0] == '\0' || pStoragePath == nullptr || pStoragePath[0] == '\0')
		return false;
	if(!Storage()->FileExists(pSourcePath, SourceStorageType))
		return false;
	if(!AudioPackEditorEnsureStorageDirectories(pStoragePath))
		return false;

	IOHANDLE SourceFile = Storage()->OpenFile(pSourcePath, IOFLAG_READ, SourceStorageType);
	if(!SourceFile)
		return false;

	void *pData = nullptr;
	unsigned DataSize = 0;
	const bool ReadOk = io_read_all(SourceFile, &pData, &DataSize);
	io_close(SourceFile);
	if(!ReadOk || pData == nullptr)
	{
		if(pData != nullptr)
			free(pData);
		return false;
	}

	IOHANDLE DestFile = Storage()->OpenFile(pStoragePath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!DestFile)
	{
		free(pData);
		return false;
	}

	const bool WriteOk = io_write(DestFile, pData, DataSize) == DataSize;
	io_close(DestFile);
	free(pData);
	return WriteOk;
}

bool CMenus::AudioPackEditorCopyAbsoluteFileToStorage(const char *pSourcePath, const char *pStoragePath)
{
	return AudioPackEditorCopyFileToStorage(pSourcePath, IStorage::TYPE_ABSOLUTE, pStoragePath);
}

void CMenus::RenderAudioPackEditorScreen(CUIRect MainView)
{
	const SSettingsContentMetrics EditorMetrics = ResolveSettingsContentMetrics(MainView.w);
	const float EditorFontSize = EditorMetrics.m_BodySize;
	const float EditorSecondaryFontSize = maximum(9.0f, EditorFontSize - 2.0f);
	const float EditorLineSize = EditorMetrics.m_LineHeight;
	const float EditorMarginSmall = EditorMetrics.m_LineSpacing;
	const float EditorMarginExtraSmall = maximum(2.0f, EditorMarginSmall * 0.5f);

	if(!m_AudioPackEditorState.m_Open)
		return;

	if(!m_AudioPackEditorState.m_Initialized)
	{
		AudioPackEditorRefreshCandidates();
		m_AudioPackEditorState.m_Initialized = true;
	}

	if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		AudioPackEditorClose();
		return;
	}

	IUiContext AudioPackSlotSearchCtx;
	AudioPackSlotSearchCtx.m_pUi = Ui();
	AudioPackSlotSearchCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();
	AudioPackSlotSearchCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();
	AudioPackSlotSearchCtx.m_ScopeHash = MakeUiScopeHash("settings_audio_pack_slot_search");
	AudioPackSlotSearchCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
	IUiContext AudioPackCandidateSearchCtx;
	AudioPackCandidateSearchCtx.m_pUi = Ui();
	AudioPackCandidateSearchCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();
	AudioPackCandidateSearchCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();
	AudioPackCandidateSearchCtx.m_ScopeHash = MakeUiScopeHash("settings_audio_pack_candidate_search");
	AudioPackCandidateSearchCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();
	IUiContext AudioPackEditorTextInputCtx;
	AudioPackEditorTextInputCtx.m_pUi = Ui();
	AudioPackEditorTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();
	AudioPackEditorTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();
	AudioPackEditorTextInputCtx.m_ScopeHash = MakeUiScopeHash("settings_audio_pack_text_inputs");
	AudioPackEditorTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();

	const auto vAllSlots = BuildAudioPackSlots();
	if(vAllSlots.empty())
	{
		DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_no_slots_label", &MainView, Localize("No audio slots found."), EditorFontSize, TEXTALIGN_MC);
		return;
	}

	m_AudioPackEditorState.m_SelectedSlotIndex = std::clamp(m_AudioPackEditorState.m_SelectedSlotIndex, 0, (int)vAllSlots.size() - 1);

	CUIRect EditorRect = MainView;
	EditorRect.Margin(8.0f, &EditorRect);
	EditorRect.Draw(ColorRGBA(0.10f, 0.11f, 0.15f, 1.0f), IGraphics::CORNER_ALL, ui_token::radius::CARD);

	CUIRect WorkRect;
	EditorRect.Margin(8.0f, &WorkRect);
	SSettingsContentMetrics EditorRowMetrics = ResolveSettingsContentMetrics(WorkRect.w);
	EditorRowMetrics.m_BodySize = EditorFontSize;

	CUIRect TopPanel, TopBarRow1, TopBarRow2, ContentRow, StatusRow;
	WorkRect.HSplitTop(EditorLineSize * 2.0f + EditorMarginSmall + 8.0f, &TopPanel, &ContentRow);
	TopPanel.HSplitTop(EditorLineSize + 4.0f, &TopBarRow1, &TopPanel);
	TopPanel.HSplitTop(EditorMarginExtraSmall, nullptr, &TopPanel);
	TopBarRow2 = TopPanel;
	ContentRow.HSplitBottom(EditorLineSize + EditorMarginSmall, &ContentRow, &StatusRow);

	static CButtonContainer s_AudioPackEditorCloseButton;
	static CButtonContainer s_AudioPackEditorRefreshButton;
	static CButtonContainer s_AudioPackEditorPreviewButton;
	static CButtonContainer s_AudioPackEditorExportButton;
	static CButtonContainer s_AudioPackEditorImportPreviewButton;
	static CListBox s_AudioPackEditorSlotListBox;
	static CListBox s_AudioPackEditorCandidateListBox;
	static std::vector<int> s_vAudioPackEditorSlotItemIds;

	auto SplitLeftSafe = [](CUIRect &Source, float Wanted, CUIRect *pLeft, CUIRect *pRight) {
		const float Cut = minimum(Wanted, Source.w);
		Source.VSplitLeft(Cut, pLeft, pRight);
	};

	CUIRect CloseButton, PackRow, TitleRow, RefreshButton;
	SplitLeftSafe(TopBarRow1, 28.0f, &CloseButton, &TopBarRow1);
	SplitLeftSafe(TopBarRow1, EditorMarginSmall, nullptr, &TopBarRow1);
	PackRow = TopBarRow1;

	CUIRect RefreshControls;
	SplitSettingsControlRow(TopBarRow2, EditorRowMetrics, &TitleRow, &RefreshControls);
	static ui_widget::SSettingsActionGroupState s_EditorRefreshWidths;
	const char *apRefreshLabels[] = {Localize("Reload")};
	RefreshButton = ui_widget::PrepareSettingsActionGroup(*Ui(), s_EditorRefreshWidths, RefreshControls, apRefreshLabels, 1, EditorRowMetrics).m_aButtons[0];

	if(Ui()->DoButton_QmIcon(&s_AudioPackEditorCloseButton, EQmIcon::CLOSE, FONT_ICON_XMARK, 0, &CloseButton, IGraphics::CORNER_ALL))
	{
		AudioPackEditorClose();
		return;
	}

	CUIRect PackLabel, PackInput;
	SplitSettingsControlRow(PackRow, EditorRowMetrics, &PackLabel, &PackInput);
	DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_pack_name_label", &PackLabel, Localize("Pack name"), EditorFontSize, TEXTALIGN_ML);
	if(ui_widget::InputField(AudioPackEditorTextInputCtx, &m_AudioPackEditorState.m_PackNameInput, PackInput, Localize("Pack name"), EditorFontSize))
		AudioPackEditorRefreshCandidates();

	DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_pack_edit_title", &TitleRow, Localize("Edit audio pack"), EditorFontSize, TEXTALIGN_ML);
	if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorRefreshButton, "sound-audio-pack-editor-reload", Localize("Reload"), 0, &RefreshButton))
		AudioPackEditorRefreshCandidates();

	ContentRow.HSplitTop(EditorMarginSmall, nullptr, &ContentRow);

	CUIRect SlotColumn, CandidateColumn, DetailColumn;
	ContentRow.VSplitLeft(260.0f, &SlotColumn, &ContentRow);
	ContentRow.VSplitLeft(8.0f, nullptr, &ContentRow);
	ContentRow.VSplitLeft(320.0f, &CandidateColumn, &ContentRow);
	ContentRow.VSplitLeft(8.0f, nullptr, &ContentRow);
	DetailColumn = ContentRow;

	CUIRect SlotSearchRow, SlotListRow;
	SlotColumn.HSplitTop(EditorLineSize, &SlotSearchRow, &SlotColumn);
	SlotColumn.HSplitTop(EditorMarginSmall, nullptr, &SlotColumn);
	SlotListRow = SlotColumn;
	CUIRect SlotSearchInput, SlotSearchLabel;
	SplitSettingsControlRow(SlotSearchRow, EditorRowMetrics, &SlotSearchLabel, &SlotSearchInput);
	DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_pack_slot_search_label", &SlotSearchLabel, Localize("Search"), EditorFontSize, TEXTALIGN_ML);
	ui_widget::InputField(AudioPackSlotSearchCtx, &m_AudioPackEditorState.m_FilterInput, SlotSearchInput, EditorFontSize, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	std::vector<int> vVisibleSlotIndices;
	vVisibleSlotIndices.reserve(vAllSlots.size());
	const char *pSlotFilter = m_AudioPackEditorState.m_FilterInput.GetString();
	for(int SlotIndex = 0; SlotIndex < (int)vAllSlots.size(); ++SlotIndex)
	{
		const auto &Slot = vAllSlots[SlotIndex];
		if(pSlotFilter[0] != '\0' &&
			!str_find_nocase(Slot.m_pDisplayName, pSlotFilter) &&
			!str_find_nocase(Slot.m_pSetName, pSlotFilter) &&
			!str_find_nocase(Slot.m_pRelativePath, pSlotFilter))
		{
			continue;
		}
		vVisibleSlotIndices.push_back(SlotIndex);
	}

	if(!vVisibleSlotIndices.empty())
	{
		if(std::find(vVisibleSlotIndices.begin(), vVisibleSlotIndices.end(), m_AudioPackEditorState.m_SelectedSlotIndex) == vVisibleSlotIndices.end())
		{
			m_AudioPackEditorState.m_SelectedSlotIndex = vVisibleSlotIndices.front();
			AudioPackEditorRefreshCandidates();
		}
	}

	s_AudioPackEditorSlotListBox.DoHeader(&SlotListRow, Localize("Audio slots"), EditorLineSize, EditorMarginExtraSmall);
	int SelectedVisibleSlot = 0;
	for(int Index = 0; Index < (int)vVisibleSlotIndices.size(); ++Index)
	{
		if(vVisibleSlotIndices[Index] == m_AudioPackEditorState.m_SelectedSlotIndex)
		{
			SelectedVisibleSlot = Index;
			break;
		}
	}
	const int OldSelectedVisibleSlot = SelectedVisibleSlot;
	s_vAudioPackEditorSlotItemIds.resize(vAllSlots.size());
	s_AudioPackEditorSlotListBox.DoStart(EditorLineSize, vVisibleSlotIndices.size(), 1, 6, SelectedVisibleSlot);
	for(int VisibleIndex = 0; VisibleIndex < (int)vVisibleSlotIndices.size(); ++VisibleIndex)
	{
		const int SlotIndex = vVisibleSlotIndices[VisibleIndex];
		const auto &Slot = vAllSlots[SlotIndex];
		const CListboxItem Item = s_AudioPackEditorSlotListBox.DoNextItem(&s_vAudioPackEditorSlotItemIds[SlotIndex], SelectedVisibleSlot == VisibleIndex);
		if(!Item.m_Visible)
			continue;

		char aLabel[256];
		if(Slot.m_VariantCount > 1)
			str_format(aLabel, sizeof(aLabel), "%s [%d/%d]", Slot.m_pSetName, Slot.m_VariantIndex + 1, Slot.m_VariantCount);
		else
			str_copy(aLabel, Slot.m_pSetName, sizeof(aLabel));
		Ui()->DoLabel(&Item.m_Rect, aLabel, EditorFontSize, TEXTALIGN_ML);
	}
	SelectedVisibleSlot = s_AudioPackEditorSlotListBox.DoEnd();
	if(SelectedVisibleSlot != OldSelectedVisibleSlot && SelectedVisibleSlot >= 0 && SelectedVisibleSlot < (int)vVisibleSlotIndices.size())
	{
		m_AudioPackEditorState.m_SelectedSlotIndex = vVisibleSlotIndices[SelectedVisibleSlot];
		AudioPackEditorRefreshCandidates();
	}

	CUIRect CandidateSearchRow, CandidateListRow;
	CandidateColumn.HSplitTop(EditorLineSize, &CandidateSearchRow, &CandidateColumn);
	CandidateColumn.HSplitTop(EditorMarginSmall, nullptr, &CandidateColumn);
	CandidateListRow = CandidateColumn;
	CUIRect CandidateSearchInput, CandidateSearchLabel;
	SplitSettingsControlRow(CandidateSearchRow, EditorRowMetrics, &CandidateSearchLabel, &CandidateSearchInput);
	DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_pack_candidate_search_label", &CandidateSearchLabel, Localize("Search"), EditorFontSize, TEXTALIGN_ML);
	ui_widget::InputField(AudioPackCandidateSearchCtx, &m_AudioPackEditorState.m_CandidateFilterInput, CandidateSearchInput, EditorFontSize, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	std::vector<int> vVisibleCandidateIndices;
	vVisibleCandidateIndices.reserve(m_AudioPackEditorState.m_vCandidateEntries.size());
	const char *pCandidateFilter = m_AudioPackEditorState.m_CandidateFilterInput.GetString();
	for(int CandidateIndex = 0; CandidateIndex < (int)m_AudioPackEditorState.m_vCandidateEntries.size(); ++CandidateIndex)
	{
		const auto &Entry = m_AudioPackEditorState.m_vCandidateEntries[CandidateIndex];
		if(pCandidateFilter[0] != '\0' &&
			!str_find_nocase(Entry.m_DisplayName.c_str(), pCandidateFilter) &&
			!str_find_nocase(Entry.m_Path.c_str(), pCandidateFilter))
		{
			continue;
		}
		vVisibleCandidateIndices.push_back(CandidateIndex);
	}

	if(!vVisibleCandidateIndices.empty())
	{
		if(std::find(vVisibleCandidateIndices.begin(), vVisibleCandidateIndices.end(), m_AudioPackEditorState.m_SelectedCandidateIndex) == vVisibleCandidateIndices.end())
			m_AudioPackEditorState.m_SelectedCandidateIndex = vVisibleCandidateIndices.front();
	}
	else
	{
		m_AudioPackEditorState.m_SelectedCandidateIndex = -1;
	}

	s_AudioPackEditorCandidateListBox.DoHeader(&CandidateListRow, Localize("Candidate files"), EditorLineSize, EditorMarginExtraSmall);
	int SelectedVisibleCandidate = 0;
	for(int Index = 0; Index < (int)vVisibleCandidateIndices.size(); ++Index)
	{
		if(vVisibleCandidateIndices[Index] == m_AudioPackEditorState.m_SelectedCandidateIndex)
		{
			SelectedVisibleCandidate = Index;
			break;
		}
	}
	const int OldSelectedVisibleCandidate = SelectedVisibleCandidate;
	s_AudioPackEditorCandidateListBox.DoStart(EditorLineSize, vVisibleCandidateIndices.size(), 1, 6, SelectedVisibleCandidate);
	for(int VisibleIndex = 0; VisibleIndex < (int)vVisibleCandidateIndices.size(); ++VisibleIndex)
	{
		const int CandidateIndex = vVisibleCandidateIndices[VisibleIndex];
		const auto &Entry = m_AudioPackEditorState.m_vCandidateEntries[CandidateIndex];
		const CListboxItem Item = s_AudioPackEditorCandidateListBox.DoNextItem(&Entry, SelectedVisibleCandidate == VisibleIndex);
		if(!Item.m_Visible)
			continue;

		char aLabel[IO_MAX_PATH_LENGTH + 64];
		if(Entry.m_IsCurrentFile)
			str_format(aLabel, sizeof(aLabel), "%s (%s)", Entry.m_DisplayName.c_str(), Localize("Current file"));
		else if(Entry.m_IsCurrentPackFile)
			str_format(aLabel, sizeof(aLabel), "%s (%s)", Entry.m_DisplayName.c_str(), Localize("Pack name"));
		else
			str_copy(aLabel, Entry.m_DisplayName.c_str(), sizeof(aLabel));
		Ui()->DoLabel(&Item.m_Rect, aLabel, EditorFontSize, TEXTALIGN_ML);
	}
	SelectedVisibleCandidate = s_AudioPackEditorCandidateListBox.DoEnd();
	if(SelectedVisibleCandidate != OldSelectedVisibleCandidate && SelectedVisibleCandidate >= 0 && SelectedVisibleCandidate < (int)vVisibleCandidateIndices.size())
		m_AudioPackEditorState.m_SelectedCandidateIndex = vVisibleCandidateIndices[SelectedVisibleCandidate];

	const auto &SelectedSlot = vAllSlots[m_AudioPackEditorState.m_SelectedSlotIndex];
	const char *pPackName = ResolveAudioPackEditorPackName(m_AudioPackEditorState.m_PackNameInput, g_Config.m_SndPack);
	char aCurrentPath[IO_MAX_PATH_LENGTH] = "";
	ResolveAudioPackEditorCurrentFilePath(Storage(), pPackName, SelectedSlot, aCurrentPath, sizeof(aCurrentPath));

	const char *pSelectedCandidatePath = "";
	if(m_AudioPackEditorState.m_SelectedCandidateIndex >= 0 && m_AudioPackEditorState.m_SelectedCandidateIndex < (int)m_AudioPackEditorState.m_vCandidateEntries.size())
		pSelectedCandidatePath = m_AudioPackEditorState.m_vCandidateEntries[m_AudioPackEditorState.m_SelectedCandidateIndex].m_Path.c_str();

	CUIRect DetailRow;
	DetailColumn.HSplitTop(EditorLineSize, &DetailRow, &DetailColumn);
	char aSlotLabel[256];
	if(SelectedSlot.m_VariantCount > 1)
		str_format(aSlotLabel, sizeof(aSlotLabel), "%s [%d/%d]", SelectedSlot.m_pSetName, SelectedSlot.m_VariantIndex + 1, SelectedSlot.m_VariantCount);
	else
		str_copy(aSlotLabel, SelectedSlot.m_pSetName, sizeof(aSlotLabel));
	Ui()->DoLabel(&DetailRow, aSlotLabel, EditorFontSize, TEXTALIGN_ML);

	DetailColumn.HSplitTop(EditorLineSize, &DetailRow, &DetailColumn);
	char aRelativeLabel[256];
	str_format(aRelativeLabel, sizeof(aRelativeLabel), "%s: %s", Localize("Relative path"), SelectedSlot.m_pRelativePath);
	Ui()->DoLabel(&DetailRow, aRelativeLabel, EditorSecondaryFontSize, TEXTALIGN_ML);

	DetailColumn.HSplitTop(EditorLineSize, &DetailRow, &DetailColumn);
	char aCurrentLabel[IO_MAX_PATH_LENGTH + 32];
	if(aCurrentPath[0] != '\0')
		str_format(aCurrentLabel, sizeof(aCurrentLabel), "%s: %s", Localize("Current file"), aCurrentPath);
	else
		str_format(aCurrentLabel, sizeof(aCurrentLabel), "%s: %s", Localize("Current file"), Localize("Default"));
	Ui()->DoLabel(&DetailRow, aCurrentLabel, EditorSecondaryFontSize, TEXTALIGN_ML);

	DetailColumn.HSplitTop(EditorLineSize, &DetailRow, &DetailColumn);
	char aSelectedLabel[IO_MAX_PATH_LENGTH + 48];
	if(pSelectedCandidatePath[0] != '\0')
		str_format(aSelectedLabel, sizeof(aSelectedLabel), "%s: %s", Localize("Selected candidate"), pSelectedCandidatePath);
	else
		str_format(aSelectedLabel, sizeof(aSelectedLabel), "%s: %s", Localize("Selected candidate"), Localize("Default"));
	Ui()->DoLabel(&DetailRow, aSelectedLabel, EditorSecondaryFontSize, TEXTALIGN_ML);

	DetailColumn.HSplitTop(EditorMarginSmall * 2.0f, nullptr, &DetailColumn);
	CUIRect ManualRow;
	DetailColumn.HSplitTop(EditorLineSize, &ManualRow, &DetailColumn);
	CUIRect ManualInput, ManualLabel;
	SplitSettingsControlRow(ManualRow, EditorRowMetrics, &ManualLabel, &ManualInput);
	DoSettingsMenuLabel(SETTINGS_SOUND, -1, -1, "audio_manual_source_label", &ManualLabel, Localize("Manual source file"), EditorFontSize, TEXTALIGN_ML);
	ui_widget::InputField(AudioPackEditorTextInputCtx, &m_AudioPackEditorState.m_SourcePathInput, ManualInput, Localize("Manual source file"), EditorFontSize);

	DetailColumn.HSplitTop(EditorMarginSmall, nullptr, &DetailColumn);
	CUIRect ActionRowTop, ActionRowBottom;
	DetailColumn.HSplitTop(EditorLineSize, &ActionRowTop, &DetailColumn);
	DetailColumn.HSplitTop(EditorMarginSmall, nullptr, &DetailColumn);
	DetailColumn.HSplitTop(EditorLineSize, &ActionRowBottom, &DetailColumn);
	CUIRect PreviewButton, ImportPreviewButton, ExportButton;
	static ui_widget::SSettingsActionGroupState s_EditorPreviewWidths;
	const char *apPreviewLabels[] = {Localize("Preview selected file"), Localize("Preview import file")};
	const auto PreviewActions = ui_widget::PrepareSettingsActionGroup(*Ui(), s_EditorPreviewWidths, ActionRowTop, apPreviewLabels, 2, EditorRowMetrics);
	PreviewButton = PreviewActions.m_aButtons[0];
	ImportPreviewButton = PreviewActions.m_aButtons[1];
	ExportButton = ActionRowBottom;

	if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorPreviewButton, "sound-audio-pack-editor-preview-selected", apPreviewLabels[0], 0, &PreviewButton))
	{
		if(pSelectedCandidatePath[0] == '\0')
		{
			AudioPackEditorSetStatus(Localize("No candidate file selected."), true);
		}
		else if(!AudioPackEditorPlayPreview(pSelectedCandidatePath, IStorage::TYPE_ALL))
		{
			AudioPackEditorSetStatus(Localize("Failed to preview candidate file."), true);
		}
		else
		{
			AudioPackEditorSetStatus("", false);
		}
	}

	if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorImportPreviewButton, "sound-audio-pack-editor-preview-import", apPreviewLabels[1], 0, &ImportPreviewButton))
	{
		const char *pManualPath = m_AudioPackEditorState.m_SourcePathInput.GetString();
		const std::string PreviewPath = ResolveAudioPackPreviewPath("", pManualPath);
		if(PreviewPath.empty())
		{
			AudioPackEditorSetStatus(Localize("Source file is empty."), true);
		}
		else if(!Storage()->FileExists(PreviewPath.c_str(), IStorage::TYPE_ABSOLUTE))
		{
			AudioPackEditorSetStatus(Localize("Source file does not exist."), true);
		}
		else if(!str_endswith(PreviewPath.c_str(), ".wv"))
		{
			AudioPackEditorSetStatus(Localize("Only .wv files are supported right now."), true);
		}
		else if(!AudioPackEditorPlayPreview(PreviewPath.c_str(), IStorage::TYPE_ABSOLUTE))
		{
			AudioPackEditorSetStatus(Localize("Failed to preview import file."), true);
		}
		else
		{
			AudioPackEditorSetStatus("", false);
		}
	}

	if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorExportButton, "sound-audio-pack-editor-export-selected", Localize("Export selected file"), 0, &ExportButton))
	{
		const char *pManualPath = m_AudioPackEditorState.m_SourcePathInput.GetString();
		const std::string SourcePath = ResolveAudioPackExportSourcePath(pSelectedCandidatePath, pManualPath);
		const bool UseManualSource = pManualPath[0] != '\0';
		int SourceStorageType = IStorage::TYPE_ALL;

		if(SourcePath.empty())
		{
			AudioPackEditorSetStatus(UseManualSource ? Localize("Source file is empty.") : Localize("No candidate file selected."), true);
		}
		else
		{
			if(UseManualSource)
			{
				SourceStorageType = IStorage::TYPE_ABSOLUTE;
				if(!Storage()->FileExists(SourcePath.c_str(), SourceStorageType))
				{
					AudioPackEditorSetStatus(Localize("Source file does not exist."), true);
					goto AudioPackExportDone;
				}
				if(!str_endswith(SourcePath.c_str(), ".wv"))
				{
					AudioPackEditorSetStatus(Localize("Only .wv files are supported right now."), true);
					goto AudioPackExportDone;
				}
			}

			const std::string ExportPath = BuildAudioPackExportPath(pPackName, SelectedSlot.m_pRelativePath);
			if(AudioPackEditorCopyFileToStorage(SourcePath.c_str(), SourceStorageType, ExportPath.c_str()))
			{
				str_copy(g_Config.m_SndPack, pPackName, sizeof(g_Config.m_SndPack));
				RefreshSharedAudioPacks(Storage());
				if(GameClient()->m_Sounds.Reload())
				{
					UpdateMusicState();
					AudioPackEditorSetStatus(Localize("Audio pack exported."), false);
				}
				else
				{
					AudioPackEditorSetStatus(Localize("Audio file was exported, but reload failed. Restart sound to apply it."), true);
				}
				AudioPackEditorRefreshCandidates();
			}
			else
			{
				AudioPackEditorSetStatus(Localize("Failed to export audio pack file."), true);
			}
		}
	}
AudioPackExportDone:

	if(m_AudioPackEditorState.m_aStatusMessage[0] != '\0')
	{
		TextRender()->TextColor(m_AudioPackEditorState.m_StatusIsError ? ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f) : ColorRGBA(0.45f, 1.0f, 0.55f, 1.0f));
		Ui()->DoLabel(&StatusRow, m_AudioPackEditorState.m_aStatusMessage, EditorSecondaryFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

uint64_t CMenus::BuildSoundSettingsCards(const qm_card_catalog::SQmCardBuildContext &Ctx, std::vector<SSettingsCardDefinition> *pCards)
{
	static bool s_SndPackInit = false;
	static char s_aSndPack[sizeof(g_Config.m_SndPack)] = "";

	if(!s_SndPackInit)
	{
		str_copy(s_aSndPack, g_Config.m_SndPack, sizeof(s_aSndPack));
		s_SndPackInit = true;
	}

	const SSettingsContentMetrics SoundMetrics = Ctx.m_Metrics;
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

	const bool RenderOnly = Ctx.m_ReadOnly;
	const auto BuildDefinitions = [=, this](std::vector<SSettingsCardDefinition> &vCards) {
		vCards.reserve(3);
		const SSettingsCardSpec ToggleSpec{pToggleDefault->m_pStableId, Localize(pToggleDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pToggleDefault)};
		const SSettingsCardSpec VolumeSpec{pVolumeDefault->m_pStableId, Localize(pVolumeDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pVolumeDefault)};
		const SSettingsCardSpec AudioPackSpec{pAudioPackDefault->m_pStableId, Localize(pAudioPackDefault->m_pTitle), qm_card_registry::ResolveLocalizedDescription(*pAudioPackDefault)};
		const auto AddCard = [this, &vCards, RenderOnly](const SSettingsCardSpec &Spec, FSettingsCardRenderMeasured Render, std::function<bool()> IsVisible = {}, bool VisibilityController = false) {
			SSettingsCardDefinition Definition;
			Definition.m_Spec = Spec;
			Definition.m_RenderMeasured = std::move(Render);
			Definition.m_IsVisible = std::move(IsVisible);
			Definition.m_VisibilityController = VisibilityController;
			qm_settings_content::UseDeclaredContent(Definition, Ui(), TextRender());
			if(RenderOnly)
				Definition.m_ContentLayoutInput = {};
			vCards.push_back(std::move(Definition));
		};
		AddCard(ToggleSpec, [this, LineHeight, LineSpacing, RenderOnly](CUIRect &ContentRect) {
			CSettingsContentLayoutFlow Flow(ContentRect);
			const bool CanCommit = Flow.Input() && !RenderOnly && !Ui()->RenderOnly();
			CUIRect Button;
			if(Flow.Control("sound-enabled", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, -1, &g_Config.m_SndEnable, "Use sounds", Localize("Use sounds"), g_Config.m_SndEnable, &Button, SLabelProperties{}, CanCommit) && CanCommit)
				{
					g_Config.m_SndEnable ^= 1;
					UpdateMusicState();
				}
			}
			if(CanCommit)
				m_NeedRestartSound = (g_Config.m_SndEnable && !Sound()->IsSoundEnabled()) || str_comp(g_Config.m_SndPack, s_aSndPack) != 0;
			if(!g_Config.m_SndEnable)
				return;
			Flow.Space(LineSpacing);
			if(Flow.Control("SndMusic", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndMusic, "Play background music", Localize("Play background music"), g_Config.m_SndMusic, &Button) && CanCommit)
				{
					g_Config.m_SndMusic ^= 1;
					UpdateMusicState();
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndNonactiveMute", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndNonactiveMute, "Mute when not active", Localize("Mute when not active"), g_Config.m_SndNonactiveMute, &Button) && CanCommit)
				{
					g_Config.m_SndNonactiveMute ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndGame", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndGame, "Enable game sounds", Localize("Enable game sounds"), g_Config.m_SndGame, &Button) && CanCommit)
				{
					g_Config.m_SndGame ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndGun", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndGun, "Enable gun sound", Localize("Enable gun sound"), g_Config.m_SndGun, &Button) && CanCommit)
				{
					g_Config.m_SndGun ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndLongPain", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndLongPain, "Enable shout when holding fire while in water", Localize("Enable shout when holding fire while in water"), g_Config.m_SndLongPain, &Button) && CanCommit)
				{
					g_Config.m_SndLongPain ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndServerMessage", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndServerMessage, "Enable server message sound", Localize("Enable server message sound"), g_Config.m_SndServerMessage, &Button) && CanCommit)
				{
					g_Config.m_SndServerMessage ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndChat", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndChat, "Enable regular chat sound", Localize("Enable regular chat sound"), g_Config.m_SndChat, &Button) && CanCommit)
				{
					g_Config.m_SndChat ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndTeamChat", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndTeamChat, "Enable team chat sound", Localize("Enable team chat sound"), g_Config.m_SndTeamChat, &Button) && CanCommit)
				{
					g_Config.m_SndTeamChat ^= 1;
				}
			}
			Flow.Space(LineSpacing);
			if(Flow.Control("SndHighlight", LineHeight, Button))
			{
				if(DoSettingsButton_CheckBox(SETTINGS_SOUND, -1, &g_Config.m_SndHighlight, "Enable highlighted chat sound", Localize("Enable highlighted chat sound"), g_Config.m_SndHighlight, &Button) && CanCommit)
				{
					g_Config.m_SndHighlight ^= 1;
				}
			} }, {}, true);
		AddCard(VolumeSpec, [DoSoundNumericField, LineHeight, LineSpacing](CUIRect &ContentRect) {
			CSettingsContentLayoutFlow Flow(ContentRect);
			CUIRect VolumeButton;
			if(Flow.Control("sound-volume", LineHeight, VolumeButton))
				DoSoundNumericField("sound-volume", &g_Config.m_SndVolume, &g_Config.m_SndVolume, VolumeButton, Localize("Sound volume"));
			Flow.Space(LineSpacing);
			if(Flow.Control("sound-game-volume", LineHeight, VolumeButton))
				DoSoundNumericField("sound-game-volume", &g_Config.m_SndGameVolume, &g_Config.m_SndGameVolume, VolumeButton, Localize("Game sound volume"));
			Flow.Space(LineSpacing);
			if(Flow.Control("sound-chat-volume", LineHeight, VolumeButton))
				DoSoundNumericField("sound-chat-volume", &g_Config.m_SndChatVolume, &g_Config.m_SndChatVolume, VolumeButton, Localize("Chat sound volume"));
			Flow.Space(LineSpacing);
			if(Flow.Control("sound-map-volume", LineHeight, VolumeButton))
				DoSoundNumericField("sound-map-volume", &g_Config.m_SndMapVolume, &g_Config.m_SndMapVolume, VolumeButton, Localize("Map sound volume"));
			Flow.Space(LineSpacing);
			if(Flow.Control("sound-background-music-volume", LineHeight, VolumeButton))
				DoSoundNumericField("sound-background-music-volume", &g_Config.m_SndBackgroundMusicVolume, &g_Config.m_SndBackgroundMusicVolume, VolumeButton, Localize("Background music volume")); }, [] { return g_Config.m_SndEnable != 0; });
		AddCard(AudioPackSpec, [this, SoundMetrics, LineHeight, LineSpacing, BodySize, RenderOnly](CUIRect &ContentRect) {
			CSettingsContentLayoutFlow Flow(ContentRect);
			const bool CanCommit = Flow.Input() && !RenderOnly && !Ui()->RenderOnly();
			static CButtonContainer s_AudioPackRefreshButton;
			static CButtonContainer s_AudioPackEditorButton;
			static CButtonContainer s_AudioPackDirectoryButton;
			static CListBox s_AudioPackListBox;
			if(CanCommit)
				EnsureSharedAudioPacks(Storage());

			auto RefreshAudioPackState = [&]() {
				if(!CanCommit)
					return;
				RefreshSharedAudioPacks(Storage());
				if(g_Config.m_SndPack[0] == '\0')
					str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));
				if(m_AudioPackEditorState.m_PackNameInput.IsEmpty())
					m_AudioPackEditorState.m_PackNameInput.Set(g_Config.m_SndPack);
			};

			if(CanCommit && g_Config.m_SndPack[0] == '\0')
				str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));

			auto FindSelectedPackIndex = [&]() {
				int Result = FindAudioPackIndexByName(gs_vAudioPacks, g_Config.m_SndPack);
				if(CanCommit && Result < 0)
				{
					RefreshSharedAudioPacks(Storage());
					Result = FindAudioPackIndexByName(gs_vAudioPacks, g_Config.m_SndPack);
				}
				if(CanCommit && Result < 0)
				{
					str_copy(g_Config.m_SndPack, "default", sizeof(g_Config.m_SndPack));
					Result = 0;
				}
				return std::max(0, Result);
			};
			int SelectedPack = FindSelectedPackIndex();

			const auto Geometry = ResolveSettingsSoundAudioPackGeometry(gs_vAudioPacks.size(), SoundMetrics);
			CUIRect HeaderRow, ListRow;
			const bool HasHeader = Flow.Control("audio-pack-actions", LineHeight + 2.0f, HeaderRow);
			Flow.Space(LineSpacing);
			const bool HasList = Flow.Control("audio-pack-list", Geometry.m_ListViewportHeight + 14.0f, ListRow);
			if(HasHeader)
			{
				HeaderRow.HSplitTop(2.0f, nullptr, &HeaderRow);
				HeaderRow.VMargin(2.0f, &HeaderRow);

				const auto HeaderActions = ResolveSettingsInputActionsLayout(HeaderRow, 1, SoundMetrics);
				CUIRect RefreshButton = HeaderActions.m_Actions.m_aButtons[0];
				HeaderRow = HeaderActions.m_Input;
				static ui_widget::SSettingsActionGroupState s_AudioPackActions;
				const char *apActionLabels[] = {Localize("Edit audio pack"), Localize("Audio pack directory")};
				const auto Actions = ui_widget::PrepareSettingsActionGroup(*Ui(), s_AudioPackActions, HeaderRow, apActionLabels, 2, SoundMetrics);
				CUIRect EditButton = Actions.m_aButtons[0];
				CUIRect DirectoryButton = Actions.m_aButtons[1];
				if(Ui()->DoButton_QmIcon(&s_AudioPackRefreshButton, EQmIcon::ARROW_ROTATE_RIGHT, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &RefreshButton, BUTTONFLAG_LEFT) && CanCommit)
				{
					RefreshAudioPackState();
					SelectedPack = FindSelectedPackIndex();
				}
				if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackEditorButton, "sound-edit-audio-pack", apActionLabels[0], 0, &EditButton, SoundMetrics) && CanCommit)
					AudioPackEditorOpen(g_Config.m_SndPack);
				if(DoSettingsButton_Menu(SETTINGS_SOUND, -1, -1, &s_AudioPackDirectoryButton, "sound-audio-pack-directory", apActionLabels[1], 0, &DirectoryButton, SoundMetrics) && CanCommit)
				{
					char aBuf[IO_MAX_PATH_LENGTH];
					Storage()->GetCompletePath(IStorage::TYPE_SAVE, "audio", aBuf, sizeof(aBuf));
					Client()->ViewFile(aBuf);
				}
			}
			if(!HasList)
				return;
			ListRow.VMargin(2.0f, &ListRow);
			ListRow.h -= 2.0f;
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
			if(CanCommit && SelectedPack != OldSelectedPack && SelectedPack >= 0 && SelectedPack < (int)gs_vAudioPacks.size())
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
	const uint64_t SoundLayoutRevision = RenderOnly;
	if(pCards != nullptr)
	{
		BuildDefinitions(*pCards);
	}
	if(!RenderOnly)
		m_NeedRestartSound = m_NeedRestartSound || str_comp(g_Config.m_SndPack, s_aSndPack) != 0;
	return SoundLayoutRevision;
}
