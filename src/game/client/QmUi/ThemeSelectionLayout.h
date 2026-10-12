#ifndef GAME_CLIENT_QMUI_THEMESELECTIONLAYOUT_H
#define GAME_CLIENT_QMUI_THEMESELECTIONLAYOUT_H

#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>

struct SThemeSelectionRowLayout
{
	CUIRect m_Icon;
	CUIRect m_Label;
};

// 名称优先保留实际文字宽度；窄卡片缩小预览，而不是把内置选项挤成省略号。
inline SThemeSelectionRowLayout ResolveThemeSelectionRowLayout(const CUIRect &Row, float NameWidth, float Spacing)
{
	const float Width = std::max(0.0f, Row.w);
	const float Gap = std::min(std::max(0.0f, Spacing), Width);
	const float IconWidth = std::min(std::max(0.0f, Row.h * 2.0f), std::max(0.0f, Width - std::max(0.0f, NameWidth) - Gap));
	const float UsedGap = IconWidth > 0.0f ? Gap : 0.0f;
	return {{Row.x, Row.y, IconWidth, Row.h}, {Row.x + IconWidth + UsedGap, Row.y, Width - IconWidth - UsedGap, Row.h}};
}

inline CLabelResult DrawThemeSelectionLabel(CUi *pUi, const CUIRect &Rect, const char *pName, float FontSize)
{
	if(pUi == nullptr || Rect.w <= 0.0f || Rect.h <= 0.0f)
		return {};
	SLabelProperties Props;
	Props.m_MaxWidth = std::max(0.0f, Rect.w);
	Props.m_EllipsisAtEnd = true;
	// 主题名称是原始数据，括号不能被设置卡片拆成悬浮说明。
	const CQmCardLabelHintScope ThemeNameHints(pUi, false);
	return pUi->DoLabel(&Rect, pName, FontSize, TEXTALIGN_ML, Props);
}

#endif
