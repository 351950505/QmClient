#include "QmCardCatalogInternal.h"

#include <engine/shared/config.h>

#include <game/client/QmUi/UiButtons.h>
#include <game/client/components/menus.h>
#include <game/client/components/qmclient/chat_command_hud_render.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderQmChatCompletionContent(CUIRect &Content, const SSettingsContentMetrics &Metrics, bool PrewarmOnly)
{
	const bool StartedRenderOnly = PrewarmOnly && !Ui()->RenderOnly();
	if(StartedRenderOnly)
		Ui()->BeginRenderOnly();
	const bool ReadOnly = Ui()->RenderOnly();
	struct SControls
	{
		CButtonContainer m_Background;
		CButtonContainer m_Text;
		CButtonContainer m_SelectedText;
		CButtonContainer m_Selection;
		ui_widget::SNumericFieldState m_Opacity;
		CButtonContainer m_Reset;
	};
	static SControls s_Controls;
	DoLine_ColorPicker(&s_Controls.m_Background, Metrics, &Content, Localize("Background color"), &g_Config.m_QmChatCompletionBgColor,
		color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmChatCompletionBgColor)), false);

	CUIRect Row;
	Content.HSplitTop(Metrics.m_LineHeight, &Row, &Content);
	ui_widget::SNumericFieldOptions OpacityOptions;
	OpacityOptions.m_pLabel = Localize("Background opacity");
	OpacityOptions.m_pSuffix = "%";
	OpacityOptions.m_FontSize = Metrics.m_BodySize;
	const IUiContext Ctx = SettingsUiContext("chat_completion_appearance", Metrics.m_UiScale);
	ui_widget::NumericField(Ctx, &s_Controls.m_Opacity, &s_Controls.m_Opacity, &g_Config.m_QmChatCompletionOpacity, 0, 100, Row, OpacityOptions);
	Ui()->DoConfigTooltip(&s_Controls.m_Opacity, &Row, &g_Config.m_QmChatCompletionOpacity);
	Content.HSplitTop(Metrics.m_LineSpacing, nullptr, &Content);

	DoLine_ColorPicker(&s_Controls.m_Text, Metrics, &Content, Localize("Text color"), &g_Config.m_QmChatCompletionTextColor,
		color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmChatCompletionTextColor, true)), false, nullptr, true);
	DoLine_ColorPicker(&s_Controls.m_SelectedText, Metrics, &Content, Localize("Selected text color"), &g_Config.m_QmChatCompletionSelectedTextColor,
		color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmChatCompletionSelectedTextColor, true)), false, nullptr, true);
	DoLine_ColorPicker(&s_Controls.m_Selection, Metrics, &Content, Localize("Selection background color"), &g_Config.m_QmChatCompletionSelectedColor,
		color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmChatCompletionSelectedColor, true)), false, nullptr, true);

	// 预览使用实际补全布局和渲染，选中项与普通项共享聊天中的配色语义。
	CQmChatCommandHud Preview;
	Preview.BeginUpdate("/", 1, true, 0);
	Preview.AddServerCommand("addweapon", "i[weapon-id]", Localize("Weapon"));
	Preview.AddServerCommand("info", "", Localize("Info"));
	Preview.EndUpdate();
	Preview.MoveSelection(1, "/", 1);
	const float FontSize = Metrics.m_BodySize / CQmChatCommandHud::UI_SCALE;
	const float Width = Content.w / CQmChatCommandHud::UI_SCALE;
	Preview.SetLayout(Content.x, Content.y, Width, 1000.0f, FontSize, Content.x, Content.x + Content.w);
	Content.HSplitTop(Preview.Layout().m_H, &Row, &Content);
	Preview.SetLayout(Row.x, Row.y + Row.h, Width, Row.h + Metrics.m_LineHeight, FontSize, Row.x, Row.x + Row.w);
	if(!ReadOnly)
		QmRenderChatCommandHud(Preview, Ui(), TextRender(), Ui()->MousePos());
	Content.HSplitTop(Metrics.m_LineSpacing, nullptr, &Content);

	Content.HSplitTop(Metrics.m_LineHeight, &Row, &Content);
	if(ui_widget::SecondaryButton(Ctx, &s_Controls.m_Reset, Localize("Reset chat completion appearance"), Row, ReadOnly))
	{
		g_Config.m_QmChatCompletionBgColor = DefaultConfig::QmChatCompletionBgColor;
		g_Config.m_QmChatCompletionOpacity = DefaultConfig::QmChatCompletionOpacity;
		g_Config.m_QmChatCompletionTextColor = DefaultConfig::QmChatCompletionTextColor;
		g_Config.m_QmChatCompletionSelectedTextColor = DefaultConfig::QmChatCompletionSelectedTextColor;
		g_Config.m_QmChatCompletionSelectedColor = DefaultConfig::QmChatCompletionSelectedColor;
	}
	Content.HSplitTop(Metrics.m_LineSpacing, nullptr, &Content);
	if(StartedRenderOnly)
		Ui()->EndRenderOnly();
}

namespace qm_card_catalog
{
	bool BuildChatCompletionCard(const SQmCardBuildContext &Ctx, SSettingsCardDefinition &Out)
	{
		const auto RenderChatCompletion = [pMenus = Ctx.m_pMenus, Metrics = Ctx.m_Metrics](CUIRect &Content, const bool PrewarmOnly) {
			QmCardRenderHook::RenderQmChatCompletionContent(pMenus, Content, Metrics, PrewarmOnly);
		};
		MakeModuleCard(Ctx, qm_module::EQmModuleId::ChatCompletion, "qm:chat_completion", "Chat completion appearance", "Colors for chat command and emoji completion",
			[RenderChatCompletion, ReadOnly = Ctx.m_ReadOnly](CUIRect &Content) { RenderChatCompletion(Content, ReadOnly); },
			[RenderChatCompletion](float ContentWidth) {
				CUIRect Probe{0.0f, 0.0f, ContentWidth, 9999.0f};
				RenderChatCompletion(Probe, true);
				return Probe.y;
			}, 0u, {}, Out);
		return true;
	}
}
