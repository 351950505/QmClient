#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/components/tclient/menus_tclient_internal.h>

#include <algorithm>

using namespace qm_tclient_menu;

void CMenus::BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)
{
	Tab = std::clamp(Tab, 0, NUMBER_OF_QMCLIENT_SETTINGS_TABS - 1);

	const int PreviousTab = m_QmClientSettingsTab;
	const int PreviousSettingsPage = g_Config.m_UiSettingsPage;
	const bool PreviousCollecting = m_MenuTextPlanCollecting;
	std::vector<SMenuTextPlanItem> *pPreviousCollection = m_pMenuTextPlanCollection;
	const bool PreviousPendingActive = m_MenuTextPlanPendingActive;
	SMenuTextPlanItem PreviousPendingItem;
	if(PreviousPendingActive)
		PreviousPendingItem = m_MenuTextPlanPendingItem;

	g_Config.m_UiSettingsPage = SETTINGS_QMCLIENT;
	m_QmClientSettingsTab = Tab;
	m_MenuTextPlanCollecting = true;
	m_pMenuTextPlanCollection = &vItems;
	m_MenuTextPlanPendingActive = false;
	Ui()->BeginRenderOnly();
	RenderSettings(MainView);
	Ui()->EndRenderOnly();
	if(PreviousPendingActive)
		m_MenuTextPlanPendingItem = PreviousPendingItem;
	m_MenuTextPlanPendingActive = PreviousPendingActive;
	m_pMenuTextPlanCollection = pPreviousCollection;
	m_MenuTextPlanCollecting = PreviousCollecting;
	m_QmClientSettingsTab = PreviousTab;
	g_Config.m_UiSettingsPage = PreviousSettingsPage;
}

void CMenus::BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)
{
	Tab = CanonicalizePersistedTClientTab(Tab);
	const int PreviousTab = m_TClientSettingsTab;
	const int PreviousSettingsPage = g_Config.m_UiSettingsPage;
	const bool PreviousCollecting = m_MenuTextPlanCollecting;
	std::vector<SMenuTextPlanItem> *pPreviousCollection = m_pMenuTextPlanCollection;
	const bool PreviousPendingActive = m_MenuTextPlanPendingActive;
	SMenuTextPlanItem PreviousPendingItem;
	if(PreviousPendingActive)
		PreviousPendingItem = m_MenuTextPlanPendingItem;

	g_Config.m_UiSettingsPage = SETTINGS_TCLIENT;
	m_TClientSettingsTab = Tab;
	m_MenuTextPlanCollecting = true;
	m_pMenuTextPlanCollection = &vItems;
	m_MenuTextPlanPendingActive = false;
	Ui()->BeginRenderOnly();
	RenderSettings(MainView);
	Ui()->EndRenderOnly();
	if(PreviousPendingActive)
		m_MenuTextPlanPendingItem = PreviousPendingItem;
	m_MenuTextPlanPendingActive = PreviousPendingActive;
	m_pMenuTextPlanCollection = pPreviousCollection;
	m_MenuTextPlanCollecting = PreviousCollecting;
	m_TClientSettingsTab = PreviousTab;
	g_Config.m_UiSettingsPage = PreviousSettingsPage;
}
