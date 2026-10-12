#include "SettingsNavigation.h"

#include <base/str.h>

#include <engine/shared/localization.h>

#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/cards/QmCardCatalog.h>
#include <game/client/components/menus.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace qm_settings_navigation
{
namespace
{
bool Same(const char *pLeft, const char *pRight)
{
	return pLeft != nullptr && pRight != nullptr && str_comp(pLeft, pRight) == 0;
}

SRoute Route(int Page, int SubTab = 0)
{
	const auto &vViews = Views(Page);
	if(SubTab < 0 || SubTab >= static_cast<int>(vViews.size()))
		SubTab = 0;
	return {Page, SubTab, vViews.empty() ? nullptr : vViews[SubTab].m_pId};
}

SRoute DefaultCardRoute(const qm_card_registry::SCardDefault &Card)
{
	const char *pId = Card.m_pStableId;
	if(Same(pId, "deck:general-language"))
		return Route(CMenus::SETTINGS_LANGUAGE);
	if(Same(pId, "qm:input_overlay"))
		return Route(CMenus::SETTINGS_CONTROLS, 2);
	if(Same(pId, "qm:bind_editor"))
		return Route(CMenus::SETTINGS_CONTROLS, 2);
	if(Same(pId, "qm:friend_notify"))
		return Route(CMenus::SETTINGS_FRIENDS, 2);
	if(Same(pId, "qm:focus_mode"))
		return Route(CMenus::SETTINGS_HUD);
	if(Same(pId, "qm:voice"))
		return Route(CMenus::SETTINGS_SOUND);
	if(Same(pId, "tclient:font") || Same(pId, "tclient:cursor"))
		return Route(CMenus::SETTINGS_APPEARANCE, 1);
	if(Same(pId, "tclient:visual-effects") || Same(pId, "tclient:tile-outlines") || Same(pId, "tclient:background-draw") || Same(pId, "qm:entity_overlay") || Same(pId, "qm:collision_hitbox") || Same(pId, "qm:water_hammer") || Same(pId, "qm:camera_view") || Same(pId, "qm:weapon_animation"))
		return Route(CMenus::SETTINGS_APPEARANCE);
	if(Same(pId, "qm:skin_transition") || Same(pId, "tclient:visual-nameplates") || Same(pId, "tclient:pet") || Same(pId, "tclient:rainbow") || Same(pId, "tclient:tee-trails") || Same(pId, "tclient:finish-name") || Same(pId, "tclient:player-indicator"))
		return Route(CMenus::SETTINGS_TEE, 2);
	if(Same(pId, "tclient:hud") || Same(pId, "tclient:tee-status-bar") || Same(pId, "qm:better_scoreboard"))
		return Route(CMenus::SETTINGS_HUD);
	if(Same(pId, "tclient:input") || Same(pId, "qm:key_binds") || Same(pId, "qm:pie_menu"))
		return Route(CMenus::SETTINGS_CONTROLS, 1);
	if(Same(pId, "deck:graphics-visual") || Same(pId, "deck:graphics-icons") || Same(pId, "deck:graphics-interaction") || Same(pId, "qm:ime"))
		return Route(CMenus::SETTINGS_APPEARANCE, 1);
	if(Same(pId, "deck:general-client") || Same(pId, "deck:ddnet-background"))
		return Route(CMenus::SETTINGS_APPEARANCE, 1);
	if(Same(pId, "deck:tclient-info-files"))
		return Route(CMenus::SETTINGS_QMCLIENT, 4);
	const char *pTab = Card.m_pDefaultTab;
	if(Same(pTab, "tee") || Same(pTab, "tee7"))
		return Route(CMenus::SETTINGS_TEE);
	if(Same(pTab, "player"))
		return Route(CMenus::SETTINGS_TEE, 1);
	if(Same(pTab, "appearance-name-plate"))
		return Route(CMenus::SETTINGS_TEE, 2);
	if(Same(pTab, "appearance-hud") || Same(pTab, "appearance-chat") || Same(pTab, "appearance-info-messages") || Same(pTab, "hud") || Same(pTab, "tclient-status-bar"))
		return Route(CMenus::SETTINGS_HUD);
	if(Same(pTab, "appearance-hook-collision") || Same(pTab, "appearance-laser"))
		return Route(CMenus::SETTINGS_APPEARANCE);
	return ResolveCardTab(pTab);
}
}

const std::vector<int> &PageOrder()
{
	static const std::vector<int> s_vPages = {
		CMenus::SETTINGS_LANGUAGE, CMenus::SETTINGS_FRIENDS, CMenus::SETTINGS_TEE,
		CMenus::SETTINGS_APPEARANCE, CMenus::SETTINGS_ASSETS, CMenus::SETTINGS_SOUND,
		CMenus::SETTINGS_CONTROLS, CMenus::SETTINGS_HUD, CMenus::SETTINGS_QMCLIENT,
		CMenus::SETTINGS_SEARCH, CMenus::SETTINGS_CONTRIBUTORS};
	return s_vPages;
}

const char *PageLabel(const int Page)
{
	switch(Page)
	{
	case CMenus::SETTINGS_LANGUAGE: return Localizable("Language");
	case CMenus::SETTINGS_FRIENDS: return Localizable("Friends");
	case CMenus::SETTINGS_TEE: return Localizable("Tee");
	case CMenus::SETTINGS_APPEARANCE: return Localizable("Visual");
	case CMenus::SETTINGS_ASSETS: return Localizable("Resources");
	case CMenus::SETTINGS_SOUND: return Localizable("Sound");
	case CMenus::SETTINGS_CONTROLS: return Localizable("Controls");
	case CMenus::SETTINGS_HUD: return Localizable("Game interface");
	case CMenus::SETTINGS_QMCLIENT: return Localizable("Features");
	case CMenus::SETTINGS_SEARCH: return Localizable("Search");
	case CMenus::SETTINGS_CONTRIBUTORS: return Localizable("Contributors");
	default: return "";
	}
}

const std::vector<SView> &Views(const int Page)
{
	static const std::map<int, std::vector<SView>> s_Views = {
		{CMenus::SETTINGS_LANGUAGE, {{"modern-language", Localizable("Language")}}},
		{CMenus::SETTINGS_FRIENDS, {{"modern-friends", Localizable("Friends")}, {"modern-friends-warlist", Localizable("War List")}, {"modern-friends-notifications", Localizable("Friend notify")}}},
		{CMenus::SETTINGS_TEE, {{"modern-tee", Localizable("Tee")}, {"modern-tee-identity", Localizable("Player")}, {"modern-tee-effects", Localizable("Tee appearance")}, {"modern-tee-profiles", Localizable("Profiles")}}},
		{CMenus::SETTINGS_APPEARANCE, {{"modern-visual-graphics", Localizable("Graphics")}, {"modern-visual-ui", Localizable("UI/UX")}}},
		{CMenus::SETTINGS_SOUND, {{"modern-sound", Localizable("Sound")}}},
		{CMenus::SETTINGS_CONTROLS, {{"modern-controls-binds", Localizable("Official binds")}, {"modern-controls-workshop", Localizable("Command workshop")}, {"modern-controls-overlay", Localizable("Input visualization")}}},
		{CMenus::SETTINGS_HUD, {{"modern-hud", Localizable("Game interface")}}},
		{CMenus::SETTINGS_QMCLIENT, {{"modern-features-qm", Localizable("QmClient")}, {"modern-features-game", Localizable("Game")}, {"modern-features-ddnet", Localizable("DDNet")}, {"modern-features-tclient", Localizable("TClient")}, {"modern-features-config", Localizable("Configuration")}}},
	};
	static const std::vector<SView> s_vEmpty;
	const auto It = s_Views.find(Page);
	return It != s_Views.end() ? It->second : s_vEmpty;
}

SRoute ResolveCardTab(const char *pTab)
{
	if(pTab == nullptr || pTab[0] == '\0')
		return {};
	for(const int Page : PageOrder())
	{
		const auto &vViews = Views(Page);
		for(size_t Index = 0; Index < vViews.size(); ++Index)
			if(Same(pTab, vViews[Index].m_pId))
				return Route(Page, static_cast<int>(Index));
	}
	if(Same(pTab, "general")) return Route(CMenus::SETTINGS_QMCLIENT, 1);
	if(Same(pTab, "player")) return Route(CMenus::SETTINGS_TEE, 1);
	if(Same(pTab, "tee") || Same(pTab, "tee7")) return Route(CMenus::SETTINGS_TEE);
	if(Same(pTab, "visual") || Same(pTab, "qmclient-overview")) return Route(CMenus::SETTINGS_APPEARANCE, 1);
	if(Same(pTab, "graphics") || Same(pTab, "appearance-hook-collision") || Same(pTab, "appearance-laser")) return Route(CMenus::SETTINGS_APPEARANCE);
	if(Same(pTab, "appearance-name-plate")) return Route(CMenus::SETTINGS_TEE, 2);
	if(Same(pTab, "appearance-hud") || Same(pTab, "appearance-chat") || Same(pTab, "appearance-info-messages") || Same(pTab, "hud") || Same(pTab, "tclient-status-bar")) return Route(CMenus::SETTINGS_HUD);
	if(Same(pTab, "sound")) return Route(CMenus::SETTINGS_SOUND);
	if(Same(pTab, "controls")) return Route(CMenus::SETTINGS_CONTROLS);
	if(Same(pTab, "bind") || Same(pTab, "qmclient-bind") || Same(pTab, "tclient-bind-wheel") || Same(pTab, "tclient-chat-binds")) return Route(CMenus::SETTINGS_CONTROLS, 1);
	if(Same(pTab, "function")) return Route(CMenus::SETTINGS_QMCLIENT);
	if(Same(pTab, "ddnet")) return Route(CMenus::SETTINGS_QMCLIENT, 2);
	if(Same(pTab, "tclient")) return Route(CMenus::SETTINGS_QMCLIENT, 3);
	if(Same(pTab, "tclient-warlist")) return Route(CMenus::SETTINGS_FRIENDS, 1);
	if(Same(pTab, "tclient-profiles")) return Route(CMenus::SETTINGS_TEE, 3);
	if(Same(pTab, "tclient-configs")) return Route(CMenus::SETTINGS_QMCLIENT, 4);
	if(Same(pTab, "credits-qmclient") || Same(pTab, "qmclient-contributors")) return {CMenus::SETTINGS_CONTRIBUTORS, 0, "credits-qmclient"};
	if(Same(pTab, "credits-links") || Same(pTab, "credits-other") || Same(pTab, "tclient-info") || Same(pTab, "qmclient-contributors-ddnet")) return {CMenus::SETTINGS_CONTRIBUTORS, 1, "credits-links"};
	if(Same(pTab, "global-search")) return {CMenus::SETTINGS_SEARCH, 0, "global-search"};
	return {};
}

SRoute ResolveLegacyPage(const int Page, const int AppearanceTab, const int TClientTab, const int QmTab)
{
	switch(Page)
	{
	case CMenus::SETTINGS_GENERAL: return ResolveCardTab("general");
	case CMenus::SETTINGS_PLAYER: return ResolveCardTab("player");
	case CMenus::SETTINGS_GRAPHICS: return ResolveCardTab("graphics");
	case CMenus::SETTINGS_DDNET: return ResolveCardTab("ddnet");
	case CMenus::SETTINGS_PROFILES: return ResolveCardTab("tclient-profiles");
	case CMenus::SETTINGS_CONFIGS: return ResolveCardTab("tclient-configs");
	case CMenus::SETTINGS_APPEARANCE:
	{
		const char *apTabs[] = {"appearance-hud", "appearance-chat", "appearance-name-plate", "appearance-hook-collision", "appearance-info-messages", "appearance-laser"};
		return ResolveCardTab(apTabs[std::clamp(AppearanceTab, 0, 5)]);
	}
	case CMenus::SETTINGS_TCLIENT:
	{
		const char *apTabs[] = {"tclient", "tclient-bind-wheel", "tclient-warlist", "tclient-chat-binds", "tclient-status-bar"};
		return ResolveCardTab(apTabs[std::clamp(TClientTab, 0, 4)]);
	}
	case CMenus::SETTINGS_QMCLIENT:
	{
		const char *apTabs[] = {"visual", "function", "hud", "credits-qmclient", "tclient-configs", "bind"};
		return ResolveCardTab(apTabs[std::clamp(QmTab, 0, 5)]);
	}
	default:
		return std::find(PageOrder().begin(), PageOrder().end(), Page) != PageOrder().end() ? Route(Page) : SRoute{};
	}
}

SRoute ResolveCard(const char *pStableId, const char *pSavedTab)
{
	const SRoute SavedRoute = ResolveCardTab(pSavedTab);
	if(pSavedTab != nullptr && str_startswith(pSavedTab, "modern-") != nullptr)
		return SavedRoute;
	const auto *pDefault = qm_card_registry::FindByStableId(pStableId);
	if(pDefault == nullptr)
		return SavedRoute;
	// 用户已跨页搬动的卡继续跟随保存的位置；默认位置才按新语义分类。
	if(pSavedTab != nullptr && !Same(pSavedTab, pDefault->m_pDefaultTab))
		return SavedRoute;
	if(pDefault->m_pNavigationStableId != nullptr)
	{
		const auto *pHost = qm_card_registry::FindByStableId(pDefault->m_pNavigationStableId);
		if(pHost != nullptr)
			return DefaultCardRoute(*pHost);
	}
	return DefaultCardRoute(*pDefault);
}

bool MigrateLayout(qm_card_order::CModel &Model)
{
	struct SMove
	{
		const char *m_pStableId;
		const char *m_pTarget;
		int m_Column;
		int m_Order;
		int m_SourceGroup;
	};
	std::vector<SMove> vMoves;
	std::map<std::string, int> SourceGroups;
	for(int Index = 0; Index < Model.Count(); ++Index)
	{
		const auto &Entry = Model.Entry(Index);
		const SRoute Target = ResolveCard(Entry.m_pStableId, Entry.m_pDefaultTab);
		if(Target.m_pViewId == nullptr || Same(Entry.m_pDefaultTab, Target.m_pViewId) || str_startswith(Target.m_pViewId, "modern-") == nullptr)
			continue;
		const std::string Source = Entry.m_pDefaultTab != nullptr ? Entry.m_pDefaultTab : "";
		const int Group = SourceGroups.emplace(Source, static_cast<int>(SourceGroups.size())).first->second;
		vMoves.push_back({Entry.m_pStableId, Target.m_pViewId, Entry.m_Column, Entry.m_OrderInColumn, Group});
	}
	std::stable_sort(vMoves.begin(), vMoves.end(), [](const SMove &Left, const SMove &Right) {
		if(Left.m_SourceGroup != Right.m_SourceGroup)
			return Left.m_SourceGroup < Right.m_SourceGroup;
		return Left.m_Order < Right.m_Order;
	});
	for(const auto &Move : vMoves)
	{
		const int Order = static_cast<int>(Model.ColumnIndices(Move.m_pTarget, Move.m_Column).size());
		Model.MoveToTab(Move.m_pStableId, Move.m_pTarget, Move.m_Column, Order);
	}
	return !vMoves.empty();
}

bool CardAvailable(const char *pStableId, const bool Sixup)
{
	if(Same(pStableId, "deck:tee7-editor"))
		return Sixup;
	return !Sixup || (!Same(pStableId, "deck:tee-skin-list") && !Same(pStableId, "deck:tee-skin-options") && !Same(pStableId, "deck:tee-skin-queue"));
}

std::vector<const char *> CardIdsForView(const char *pViewId, const qm_card_order::CModel &Model, const bool Sixup)
{
	std::vector<const char *> vIds;
	for(const auto &Default : qm_card_registry::Defaults())
	{
		if(!qm_card_catalog::HasCardModule(Default.m_pStableId))
			continue;
		if(!CardAvailable(Default.m_pStableId, Sixup))
			continue;
		const int Index = Model.FindByStableId(Default.m_pStableId);
		const SRoute Target = ResolveCard(Default.m_pStableId, Index >= 0 ? Model.Entry(Index).m_pDefaultTab : Default.m_pDefaultTab);
		if(Same(Target.m_pViewId, pViewId))
			vIds.push_back(Default.m_pStableId);
	}
	return vIds;
}
}
