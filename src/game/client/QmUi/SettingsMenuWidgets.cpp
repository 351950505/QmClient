#include <base/math.h>
#include <base/str.h>

#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/QmUi/QmScroll.h>
#include <game/client/QmUi/UiButtons.h>
#include <game/client/QmUi/UiForms.h>
#include <game/client/QmUi/UiSurface.h>
#include <game/client/QmUi/UiSurfaceText.h>
#include <game/client/QmUi/cards/QmCardCatalogInternal.h>
#include <game/client/QmUi/cards/QmCardCatalogTClientInternal.h>
#include <game/client/components/binds.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/qm_icon.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>

using namespace qm_tclient_cards;

std::unordered_map<std::string, CBindSlot> g_CommandBindCache;
bool g_CommandBindCacheInitialized = false;

CMenus::SSettingsQmScrollFrame CMenus::BeginSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer, CUIRect *pView, float ContentHeight, const SQmSettingsCardStyle &CardStyle, float UiScale, float PreviousOffsetY, bool Enabled)
{
	SSettingsQmScrollFrame Frame;
	Frame.m_ViewRect = *pView;
	Frame.m_ClipRect = *pView;
	Frame.m_PreviousOffsetY = PreviousOffsetY;
	Frame.m_Enabled = Enabled;
	const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy({EQmScrollProfile::SETTINGS_OUTER}, UiScale, g_Config.m_UiSmoothScrollTime / 1000.0f);
	Frame.m_Style = ScrollPolicy.m_Style;
	(void)CardStyle;
	if(!Enabled)
		return Frame;

	const SQmScrollConfig ScrollConfig = QmSettingsScrollConfig(UiScale, g_Config.m_UiSmoothScrollTime / 1000.0f);

	SQmScrollContainerInput ScrollInput;
	ScrollInput.m_Hovered = Ui()->MouseHovered(pView);
	ScrollInput.m_MouseValid = true;
	ScrollInput.m_MouseX = Ui()->MouseX();
	ScrollInput.m_MouseY = Ui()->MouseY();
	ScrollInput.m_MouseDown = Ui()->MouseButton(0);
	ScrollInput.m_MousePressed = Ui()->MouseButtonClicked(0);

	const SQmScrollContainerFrame ProbeFrame = ScrollContainer.PreviewFrame(ScrollState, *pView, ContentHeight, Frame.m_Style);
	CUIRect WheelHotRect = ProbeFrame.m_ClipRect;
	if(ProbeFrame.m_ScrollbarVisible)
		WheelHotRect.w += Frame.m_Style.m_ScrollbarWidth;
	ScrollInput.m_Hovered = Ui()->MouseHovered(&WheelHotRect);
	ScrollInput.m_ModifierPressed = Input()->ModifierIsPressed();
	ScrollInput.m_AltPressed = Input()->AltIsPressed();

	if(ProbeFrame.m_ScrollbarVisible)
	{
		const void *pScrollbarId = &ScrollContainer;
		ScrollInput.m_ThumbHovered = Ui()->MouseHovered(&ProbeFrame.m_ScrollbarThumbRect);
		ScrollInput.m_TrackHovered = Ui()->MouseHovered(&ProbeFrame.m_ScrollbarTrackRect) && !ScrollInput.m_ThumbHovered;
		if(ScrollInput.m_ThumbHovered || ScrollInput.m_TrackHovered)
			Ui()->SetHotItem(pScrollbarId);
		if((Ui()->HotItem() == pScrollbarId || ScrollInput.m_ThumbHovered || ScrollInput.m_TrackHovered) && ScrollInput.m_MousePressed)
			Ui()->SetActiveItem(pScrollbarId);
		if(Ui()->CheckActiveItem(pScrollbarId))
		{
			ScrollInput.m_ThumbHovered = ScrollInput.m_ThumbHovered || ScrollContainer.ScrollbarDragActive(ScrollState);
			ScrollInput.m_TrackHovered = ScrollInput.m_TrackHovered && !ScrollContainer.ScrollbarDragActive(ScrollState);
			if(!ScrollInput.m_MouseDown)
				Ui()->SetActiveItem(nullptr);
		}
	}

	Frame.m_Frame = ScrollContainer.Update(ScrollState, *pView, ContentHeight, GameClient()->UiRuntimeV2()->FrameDt(), ScrollInput, Frame.m_Style, ScrollConfig);
	Frame.m_ClipRect = Frame.m_Frame.m_ClipRect;
	Frame.m_Offset.y = -Frame.m_Frame.m_Offset;
	*pView = Frame.m_ClipRect;
	Ui()->ClipEnable(&Frame.m_ClipRect);
	return Frame;
}

void CMenus::RenderQmSettingsSliderWithValueInput(const void *pId, const CUIRect &ControlColumn, int *pValue, int MinValue, int MaxValue, const char *pSuffix, bool PrewarmOnly, unsigned Flags)
{
	const int OriginalValue = *pValue;
	ui_widget::SNumericFieldState *pState = GetSettingsNumericFieldState(pId);
	ui_widget::SNumericFieldOptions Options;
	Options.m_Flags = Flags;
	Options.m_pSuffix = pSuffix;
	Options.m_FontSize = CurrentSettingsContentMetrics().m_BodySize;
	Options.m_CommitPolicy = (Flags & CUi::SCROLLBAR_OPTION_DELAYUPDATE) != 0 ? ui_widget::EInputCommitPolicy::ON_RELEASE_OR_SUBMIT : ui_widget::EInputCommitPolicy::LIVE;

	const float UiScale = std::clamp(ControlColumn.h / ui_token::settings::ROW_HEIGHT, 0.78f, 1.0f);
	IUiContext InputCtx = SettingsUiContext("qmclient_slider_input", UiScale);
	if(PrewarmOnly)
	{
		InputCtx.m_pAnim = nullptr;
		InputCtx.m_pTree = nullptr;
	}
	ui_widget::NumericField(InputCtx, pState, pId, pValue, MinValue, MaxValue, ControlColumn, Options);
	if(PrewarmOnly || Ui()->RenderOnly())
		*pValue = OriginalValue;
}

// 被禅模式/Gores 等临时接管的配置项：设置页灰化显示并提示接管来源；未被接管返回 nullptr。
static const char *QmTemporaryOverrideTooltip(const char *pOwnerId)
{
	if(pOwnerId == nullptr)
		return nullptr;
	if(str_comp(pOwnerId, "qm_zen_mode") == 0)
		return Localize("Controlled by Zen mode");
	if(str_comp(pOwnerId, "qm_gores_mode") == 0)
		return Localize("Controlled by Gores mode");
	return Localize("Temporarily controlled by a mode toggle");
}

const char *CMenus::TemporaryOverrideTooltip(const int *pValue) const
{
	return QmTemporaryOverrideTooltip(ConfigManager() != nullptr ? ConfigManager()->SaveValueOverrideOwner(pValue) : nullptr);
}

bool CMenus::RenderQmFunctionCheckbox(const void *pId, const char *pTextId, const char *pText, int *pValue, CUIRect *pRect, bool PrewarmOnly, const char *pTooltip)
{
	Ui()->DoConfigTooltip(pId, pRect, pValue);
	const int OriginalValue = *pValue;
	const char *pOverrideTooltip = TemporaryOverrideTooltip(pValue);
	SLabelProperties LabelProps;
	if(pOverrideTooltip != nullptr)
	{
		LabelProps.SetColor(ui_token::color::TEXT_DISABLED);
		// 灰化行用 ProcessInput=false 绘制，不会自己占 hover；补一次只读的按钮逻辑
		// 让 HotItem 指向本行，CTooltips 才会激活提示（返回值丢弃，不写值）。
		if(!PrewarmOnly && !Ui()->RenderOnly())
		{
			Ui()->DoButtonLogic(pId, 0, pRect, BUTTONFLAG_NONE);
			GameClient()->m_Tooltips.DoToolTip(pId, pRect, pOverrideTooltip);
		}
	}
	// 被临时接管的项灰化并停止响应点击：接管期间用户改它会被接管逻辑覆盖。
	const bool Changed = DoSettingsButton_CheckBox(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, QMCLIENT_SETTINGS_TAB_FUNCTION, pId, pTextId, pText, *pValue, pRect, LabelProps, pOverrideTooltip == nullptr) != 0;
	if(Changed)
		*pValue ^= 1;
	if(pTooltip != nullptr)
		GameClient()->m_Tooltips.DoToolTip(pId, pRect, pTooltip);
	if(PrewarmOnly || Ui()->RenderOnly())
		*pValue = OriginalValue;
	return Changed;
}

bool CMenus::RenderQmVisualCheckbox(CUIRect &Content, float LineHeight, float LineSpacing, const void *pId, const char *pTextId, const char *pText, int *pValue)
{
	CUIRect Row;
	Content.HSplitTop(LineHeight, &Row, &Content);
	const char *pOverrideTooltip = TemporaryOverrideTooltip(pValue);
	Ui()->DoConfigTooltip(pId, &Row, pValue);
	SLabelProperties LabelProps;
	if(pOverrideTooltip != nullptr)
	{
		LabelProps.SetColor(ui_token::color::TEXT_DISABLED);
		// 灰化行不占 hover，补一次只读的按钮逻辑让提示能激活（返回值丢弃，不写值）。
		if(!Ui()->RenderOnly())
		{
			Ui()->DoButtonLogic(pId, 0, &Row, BUTTONFLAG_NONE);
			GameClient()->m_Tooltips.DoToolTip(pId, &Row, pOverrideTooltip);
		}
	}
	const bool Changed = DoSettingsButton_CheckBox(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_VISUAL, QMCLIENT_SETTINGS_TAB_VISUAL, pId, pTextId, pText, *pValue, &Row, LabelProps, pOverrideTooltip == nullptr) != 0;
	if(Changed)
		*pValue ^= 1;
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	return Changed;
}

void CMenus::RenderQmVisualLabel(const char *pTextId, CUIRect *pRect, const char *pText, float FontSize, int TextAlign, const SLabelProperties &LabelProps)
{
	DoSettingsMenuLabel(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_VISUAL, QMCLIENT_SETTINGS_TAB_VISUAL, pTextId, pRect, pText, FontSize, TextAlign, LabelProps, (int)pRect->w);
}

void CMenus::FinishSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer, SSettingsQmScrollFrame &Frame, const CUIRect &EndRect, float *pContentHeight, float *pPreviousOffsetY, bool TrackScrollActive)
{
	if(!Frame.m_Enabled)
		return;

	Ui()->ClipDisable();
	*pContentHeight = maximum(0.0f, std::ceil(EndRect.y + EndRect.h - (Frame.m_ClipRect.y + Frame.m_Offset.y)));
	Frame.m_Frame = ScrollContainer.PreviewFrame(ScrollState, Frame.m_ViewRect, *pContentHeight, Frame.m_Style);
	CUIRect WheelHotRect = Frame.m_Frame.m_ClipRect;
	if(Frame.m_Frame.m_ScrollbarVisible)
		WheelHotRect.w += Frame.m_Style.m_ScrollbarWidth;
	const void *pWheelOwnerId = &ScrollContainer;
	const bool WheelEligible = Frame.m_Frame.m_ScrollbarVisible && !Ui()->UnderlyingScrollBlocked() && Ui()->MouseHovered(&WheelHotRect);
	Ui()->RegisterWheelOwner(pWheelOwnerId, EUiWheelOwnerPriority::PAGE, WheelHotRect, WheelEligible);
	float WheelDelta = 0.0f;
	if(Ui()->TryConsumeWheel(pWheelOwnerId, &WheelDelta))
	{
		const SQmScrollConfig ScrollConfig = QmSettingsScrollConfig(1.0f, g_Config.m_UiSmoothScrollTime / 1000.0f);
		ScrollContainer.ScrollByWheel(ScrollState, WheelDelta, Frame.m_ViewRect.h, *pContentHeight, ScrollConfig);
		Frame.m_Frame = ScrollContainer.PreviewFrame(ScrollState, Frame.m_ViewRect, *pContentHeight, Frame.m_Style);
	}
	const float CurrentOffsetY = Frame.m_Frame.m_ScrollbarVisible ? Frame.m_Frame.m_Offset : 0.0f;
	if(TrackScrollActive)
	{
		m_SettingsScrollActive = m_SettingsScrollActive || absolute(CurrentOffsetY - Frame.m_PreviousOffsetY) > 0.01f;
		if(pPreviousOffsetY != nullptr)
			*pPreviousOffsetY = CurrentOffsetY;
	}
	if(Frame.m_Frame.m_ScrollbarVisible)
	{
		DrawRoundedSurface(Ui(), Frame.m_Frame.m_ScrollbarTrackRect, ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), ColorRGBA(), Frame.m_Frame.m_ScrollbarTrackRect.w * 0.5f);
		DrawRoundedSurface(Ui(), Frame.m_Frame.m_ScrollbarThumbRect, ColorRGBA(1.0f, 1.0f, 1.0f, 0.34f), ColorRGBA(), Frame.m_Frame.m_ScrollbarThumbRect.w * 0.5f);
	}
}

bool CMenus::RenderQmHudCheckbox(CUIRect &Content, float LineHeight, float LineSpacing, const void *pId, const char *pTextId, const char *pText, int *pValue)
{
	CUIRect Row;
	Content.HSplitTop(LineHeight, &Row, &Content);
	const bool Changed = DoSettingsButton_CheckBox(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_HUD, QMCLIENT_SETTINGS_TAB_HUD, pId, pTextId, pText, *pValue, &Row) != 0;
	if(Changed)
		*pValue ^= 1;
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	return Changed;
}

bool CMenus::HandleQmHudCheckboxInput(CUIRect &Content, float LineHeight, float LineSpacing, const void *pId, int *pValue)
{
	const CUIRect VisibleContent = Content;
	CUIRect Row;
	Content.HSplitTop(LineHeight, &Row, &Content);
	const CUIRect HitRect = Row.Intersection(VisibleContent);
	const bool Changed = HitRect.w > 0.0f && HitRect.h > 0.0f && Ui()->DoButtonLogic(pId, *pValue, &HitRect, BUTTONFLAG_LEFT) != 0;
	if(Changed)
		*pValue ^= 1;
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	return Changed;
}

void CMenus::RenderQmHudLabel(const char *pTextId, CUIRect *pRect, const char *pText, float FontSize, int TextAlign, const SLabelProperties &LabelProps)
{
	DoSettingsMenuLabel(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_HUD, QMCLIENT_SETTINGS_TAB_HUD, pTextId, pRect, pText, FontSize, TextAlign, LabelProps, (int)pRect->w);
}

void CMenus::RenderQmHudKeyBindRow(CUIRect &Content, CButtonContainer &ReaderButton, CButtonContainer &ClearButton, const char *pLabel, const char *pCommand, float LineHeight, float BodySize, float LineSpacing, float LabelWidth)
{
	CBindSlot Bind(KEY_UNKNOWN, KeyModifier::NONE);
	const auto CurrentBindIt = g_CommandBindCache.find(pCommand);
	if(CurrentBindIt != g_CommandBindCache.end())
		Bind = CurrentBindIt->second;

	CUIRect BindRow, BindLabel, BindKey;
	Content.HSplitTop(LineHeight, &BindRow, &Content);
	BindRow.VSplitLeft(LabelWidth, &BindLabel, &BindKey);
	Ui()->DoLabel(&BindLabel, pLabel, BodySize, TEXTALIGN_ML);

	const auto Result = GameClient()->m_KeyBinder.DoKeyReader(&ReaderButton, &ClearButton, &BindKey, Bind, false);
	if(Result.m_Bind != Bind)
	{
		if(Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Bind.m_Key, "", false, Bind.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
		{
			GameClient()->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
			g_CommandBindCache.insert_or_assign(std::string(pCommand), Result.m_Bind);
		}
		else
		{
			g_CommandBindCache.erase(pCommand);
		}
	}
	Content.HSplitTop(LineSpacing, nullptr, &Content);
}

bool CMenus::ToggleQmHudCountdownLocation(CUIRect &Content, float LineHeight, float LineSpacing, const void *pId, int *pValue)
{
	// 本地差异：远程此入口只做按钮逻辑、不绘制标签（其卡片模块也未另画标签），
	// 而本地既有实现带可见文案（"Follow Tee" / "Show in Dynamic Island"）。
	// 按控件 id 选择对应标签，保留本地界面文案，避免迁出后标签消失。
	if(pId == qm_card_catalog::SwitchCountdownFollowTeeId())
		return RenderQmHudCheckbox(Content, LineHeight, LineSpacing, pId, "qmclient-switch-countdown-follow-tee", Localize("Follow Tee"), pValue);
	return RenderQmHudCheckbox(Content, LineHeight, LineSpacing, pId, "qmclient-switch-countdown-media-island", Localize("Show in Dynamic Island"), pValue);
}

bool CMenus::RenderQmFunctionCheckboxRow(CUIRect &Content, const float LineHeight, const float LineSpacing, const void *pId, const char *pTextId, const char *pText, int *pValue, const bool PrewarmOnly, const char *pTooltip)
{
	CUIRect Row;
	Content.HSplitTop(LineHeight, &Row, &Content);
	const bool Changed = RenderQmFunctionCheckbox(pId, pTextId, pText, pValue, &Row, PrewarmOnly, pTooltip);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	return Changed;
}

int CMenus::DoTClientSettingsButton_CheckBox(const void *pId, const char *pTextId, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoSettingsButton_CheckBox(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, pId, pTextId, pText, Checked, pRect, TClientFixedLabelProperties(TCLIENT_BODY_FONT_SIZE, pRect->w), true, TCLIENT_BODY_FONT_SIZE);
}

int CMenus::DoTClientSettingsButton_Menu(CButtonContainer *pButtonContainer, const char *pTextId, const char *pText, int Checked, const CUIRect *pRect, int Flags, int Corners, float Rounding)
{
	return DoSettingsButton_Menu(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, pButtonContainer, pTextId, pText, Checked, pRect, Flags, Corners, Rounding, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), 0.0f, TCLIENT_BODY_FONT_SIZE);
}

int CMenus::DoTClientSettingsButton_CheckBoxAutoVMarginAndSet(const void *pId, const char *pTextId, const char *pText, int *pValue, CUIRect *pRect, float VMargin)
{
	return DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_TCLIENT, m_TClientSettingsTab, pId, pTextId, pText, pValue, pRect, VMargin, 0.0f, FontSize);
}

bool CMenus::DoLine_KeyReader(CUIRect &View, CButtonContainer &ReaderButton, CButtonContainer &ClearButton, const char *pName, const char *pCommand)
{
	CBindSlot Bind(0, 0);
	if(g_CommandBindCacheInitialized)
	{
		const auto It = g_CommandBindCache.find(pCommand);
		if(It != g_CommandBindCache.end())
			Bind = It->second;
	}
	else
	{
		for(int Mod = 0; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
		{
			for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
			{
				const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
				if(!pBind[0])
					continue;

				if(str_comp(pBind, pCommand) == 0)
				{
					Bind.m_Key = KeyId;
					Bind.m_ModifierMask = Mod;
					break;
				}
			}
		}
	}

	CUIRect KeyButton, KeyLabel;
	View.HSplitTop(LineSize, &KeyButton, &View);
	KeyButton.VSplitMid(&KeyLabel, &KeyButton);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", pName);
	DoTClientLabel(Ui(), &KeyLabel, aBuf, FontSize, TEXTALIGN_ML);

	View.HSplitTop(MarginExtraSmall, nullptr, &View);

	const auto Result = GameClient()->m_KeyBinder.DoKeyReader(&ReaderButton, &ClearButton, &KeyButton, Bind, false, TCLIENT_BODY_FONT_SIZE);
	if(Result.m_Bind != Bind)
	{
		if(Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Bind.m_Key, "", false, Bind.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
		g_CommandBindCacheInitialized = false;
		return true;
	}
	return false;
}

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	ui_widget::SNumericFieldState *pState = GetSettingsNumericFieldState(pId);

	ui_widget::SNumericFieldOptions Options;
	Options.m_pLabel = pStr;
	Options.m_pSuffix = pSuffix;
	Options.m_pScale = pScale;
	Options.m_Flags = Flags;
	Options.m_FontSize = CurrentSettingsContentMetrics().m_BodySize;
	Options.m_LabelAlign = TEXTALIGN_ML;
	Options.m_ValueMultiplier = Scale;

	IUiContext InputCtx = SettingsUiContext("tclient_slider_input", Options.m_FontSize / ui_token::font::BODY);

	return ui_widget::NumericField(InputCtx, pState, pId, pOption, Min, Max, *pRect, Options);
}

int CMenus::DoButtonLineSize_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, float ButtonLineSize, bool Fake, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color, float FontSize)
{
	CUiScopedGaussianBlurSuppression GaussianBlurSuppression(Ui());
	CUIRect Text = *pRect;

	ui_widget::SButtonSurfaceOptions Options;
	Options.m_Enabled = !Fake && Checked >= 0;
	Options.m_Selected = Checked > 0;
	Options.m_Corners = Corners;
	Options.m_Radius = Rounding;
	Color = ui_widget::DrawButtonSurface(ui_widget::ControlContext(Ui()), pButtonContainer, *pRect, Options);
	CUiScopedSurfaceText SurfaceText(TextRender(), Color);

	Text.HMargin((Text.h - ButtonLineSize) / 2.0f, &Text);
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	const float EffectiveFontSize = FontSize > 0.0f ? FontSize : CurrentSettingsContentMetrics().m_BodySize;
	if(FontSize > 0.0f)
		DoTClientLabel(Ui(), &Text, pText, EffectiveFontSize, TEXTALIGN_MC);
	else
		Ui()->DoLabel(&Text, pText, EffectiveFontSize, TEXTALIGN_MC);

	if(Fake)
		return 0;

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_LEFT);
}

void CMenus::RenderFontIcon(const CUIRect Rect, const char *pText, float Size, int Align)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&Rect, pText, Size, Align);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::RenderFontIcon_QmIcon(const CUIRect Rect, EQmIcon Icon, const char *pFallbackIcon, float Size, int Align)
{
	// 图集优先；图集未就绪时 DoLabel_QmIcon 内部回退到 pFallbackIcon 字形。
	Ui()->DoLabel_QmIcon(&Rect, Icon, pFallbackIcon, Size, Align);
}

int CMenus::DoButtonNoRect_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	return ui_widget::DoIconButton(ui_widget::ControlContext(Ui()), pButtonContainer, EQmIcon::COUNT, pText, Checked, *pRect, BUTTONFLAG_LEFT, Corners, true, std::nullopt, false, true);
}

int CMenus::DoButtonNoRect_QmIcon(CButtonContainer *pButtonContainer, EQmIcon Icon, const char *pFallbackIcon, int Checked, const CUIRect *pRect, int Corners)
{
	return ui_widget::DoIconButton(ui_widget::ControlContext(Ui()), pButtonContainer, Icon, pFallbackIcon, Checked, *pRect, BUTTONFLAG_LEFT, Corners, true, std::nullopt, false, true);
}
