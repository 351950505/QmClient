#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/localization.h>

// 更好的计分板卡片内容。计分板相关配置集中在独立卡片中，避免与梦的小功能卡片耦合。
void CMenus::RenderQmFunctionBetterScoreboardContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float, bool PrewarmOnly)
{
	CUIRect Row;
	DoSettingsToggleGroup(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, QMCLIENT_SETTINGS_TAB_FUNCTION, Content,
		{
			{&g_Config.m_QmBetterScoreboard, "Better scoreboard", Localize("Better scoreboard")},
			{&g_Config.m_QmScoreboardPoints, "Scoreboard point check", Localize("Scoreboard point check")},
			{&g_Config.m_QmScoreboardOnDeath, "Show scoreboard after death", Localize("Show scoreboard after death")},
			{&g_Config.m_QmScoreboardScroll, "Scroll mode", Localize("Scroll mode"), nullptr, 0, Localize("Use the scoreboard cursor mode to scroll the list")},
		},
		CurrentSettingsContentMetrics(), !PrewarmOnly);
	{
		// 计分板过滤器直接绑定配置缓冲，保持原有输入即时生效和控制台同步语义。
		IUiContext TextInputCtx = SettingsUiContext("qmclient-mini-scoreboard-filter-input", BodySize / ui_token::font::BODY);
		// 筛选说明和输入框各占一行，长译文不会挤进狭窄的标签列。
		const char *pFilterLabel = Localize("Scoreboard filter: only show players whose name or clan contains this text");
		const float LabelHeight = std::max(LineHeight, TextRender()->TextBoundingBox(BodySize, pFilterLabel, -1, std::max(1.0f, Content.w)).m_H);
		Content.HSplitTop(LabelHeight, &Row, &Content);
		SLabelProperties Props;
		Props.m_MaxWidth = Row.w;
		Props.m_EnableWidthCheck = false;
		Props.m_MinimumFontSize = BodySize;
		DoSettingsMenuLabel(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, QMCLIENT_SETTINGS_TAB_FUNCTION, "qmclient-scoreboard-filter", &Row, pFilterLabel, BodySize, TEXTALIGN_ML, Props);
		Content.HSplitTop(LineSpacing * 0.5f, nullptr, &Content);
		Content.HSplitTop(LineHeight, &Row, &Content);
		static CLineInput s_ScoreboardFilterInput(g_Config.m_QmScoreboardFilter, sizeof(g_Config.m_QmScoreboardFilter));
		s_ScoreboardFilterInput.SetEmptyText(Localize("Leave empty to show everyone"));
		ui_widget::InputField(TextInputCtx, &s_ScoreboardFilterInput, Row, Localize("Leave empty to show everyone"), BodySize);
		Content.HSplitTop(LineSpacing, nullptr, &Content);
	}
}
