#include "ResourceEditorWorkspace.h"

#include "SettingsPageLayout.h"
#include "UiNavigation.h"

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

void CMenus::OpenResourceEditorWorkspace(int Workspace)
{
	if(Ui()->RenderOnly())
		return;
	const bool Online = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	const int PreviousPage = Online ? m_GamePage : m_MenuPage;
	if(m_ResourceEditorWorkspace.Visible() && Workspace >= 0 && Workspace != static_cast<int>(m_ResourceEditorWorkspace.Selected()))
		SuspendResourceEditorWorkspace();
	const int ReturnPage = PreviousPage == PAGE_RESOURCE_EDITOR ? m_ResourceEditorWorkspace.ReturnPage() : PreviousPage;
	m_ResourceEditorWorkspace.Enter(ReturnPage, Workspace);
	if(Online)
		SetGamePage(PAGE_RESOURCE_EDITOR);
	else
		SetMenuPage(PAGE_RESOURCE_EDITOR);
}

void CMenus::SuspendResourceEditorWorkspace()
{
	// 音频和借用槽位指针的弹窗不能跨工作区继续运行；编辑草稿继续保留。
	AudioPackEditorStopPreview();
	Ui()->ClosePopupMenu(&m_ColorPickerPopupContext);
	AssetsEditorCancelDrag();
	if(CLineInput *pInput = CLineInput::GetActiveInput())
		pInput->Deactivate();
	m_AssetsEditorState.m_FullscreenOpen = false;
	m_ResourceEditorWorkspace.Suspend();
}

void CMenus::RenderResourceEditorWorkspace(CUIRect MainView)
{
	const bool ReadOnly = Ui()->RenderOnly();
	if(!ReadOnly && !m_ResourceEditorWorkspace.Visible())
	{
		const int ReturnPage = m_ResourceEditorWorkspace.ReturnPage();
		m_ResourceEditorWorkspace.Enter(ReturnPage >= 0 && ReturnPage < PAGE_LENGTH && ReturnPage != PAGE_RESOURCE_EDITOR ? ReturnPage : PAGE_SETTINGS);
	}
	const SSettingsContentMetrics Metrics = ResolveSettingsContentMetrics(MainView.w);
	const SSettingsSubTabLayoutFrame Layout = ResolveSettingsSubTabLayout(MainView, Metrics.m_UiScale);
	static CButtonContainer s_aWorkspaceButtons[2];
	const char *apLabels[] = {Localize("Assets editor"), Localize("Edit audio pack")};
	const int Selected = static_cast<int>(m_ResourceEditorWorkspace.Selected());
	const bool BlockSwitch = m_AssetsEditorState.m_ShowExitConfirm || m_AssetsEditorState.m_FullscreenOpen || Ui()->IsPopupOpen();
	const int NewSelected = BlockSwitch ? Selected : DoNavigationTabs(s_aWorkspaceButtons, apLabels, 2, Selected, Layout.m_TabBarRect, 4.0f);
	if(!ReadOnly && !BlockSwitch && NewSelected != Selected)
	{
		AudioPackEditorStopPreview();
		Ui()->ClosePopupMenu(&m_ColorPickerPopupContext);
		AssetsEditorCancelDrag();
		if(CLineInput *pInput = CLineInput::GetActiveInput())
			pInput->Deactivate();
		m_ResourceEditorWorkspace.Select(NewSelected);
	}

	bool EditorOpen;
	if(m_ResourceEditorWorkspace.Selected() == qm_resource_editor::EWorkspace::IMAGE)
	{
		if(ReadOnly && (!m_AssetsEditorState.m_Open || !m_AssetsEditorState.m_Initialized))
			return;
		if(!m_AssetsEditorState.m_Open)
			AssetsEditorOpen(ASSETS_EDITOR_TYPE_GAME);
		RenderAssetsEditorScreen(Layout.m_ContentRect);
		EditorOpen = m_AssetsEditorState.m_Open;
	}
	else
	{
		if(ReadOnly && (!m_AudioPackEditorState.m_Open || !m_AudioPackEditorState.m_Initialized))
			return;
		if(!m_AudioPackEditorState.m_Open)
			AudioPackEditorOpen(g_Config.m_SndPack);
		RenderAudioPackEditorScreen(Layout.m_ContentRect);
		EditorOpen = m_AudioPackEditorState.m_Open;
	}
	if(!ReadOnly && !EditorOpen)
	{
		const int ReturnPage = m_ResourceEditorWorkspace.ReturnPage();
		SuspendResourceEditorWorkspace();
		if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
			SetGamePage(ReturnPage >= 0 && ReturnPage < PAGE_LENGTH && ReturnPage != PAGE_RESOURCE_EDITOR ? ReturnPage : PAGE_SETTINGS);
		else
			SetMenuPage(ReturnPage >= 0 && ReturnPage < PAGE_LENGTH && ReturnPage != PAGE_RESOURCE_EDITOR ? ReturnPage : PAGE_SETTINGS);
	}
}
