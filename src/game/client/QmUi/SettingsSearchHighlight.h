#ifndef GAME_CLIENT_QMUI_SETTINGSSEARCHHIGHLIGHT_H
#define GAME_CLIENT_QMUI_SETTINGSSEARCHHIGHLIGHT_H

#include <game/client/ui_rect.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class CUi;

struct SSettingsSearchHighlightTarget
{
	std::string m_Label;
	const void *m_pConfig = nullptr;
};

struct SSettingsSearchHighlightRect
{
	size_t m_TargetIndex = 0;
	CUIRect m_Rect{};
};

// 标签由本对象持有；矩形保留布局坐标，离开裁剪区的命中也能用于显露。
class CSettingsSearchHighlight
{
	std::vector<SSettingsSearchHighlightTarget> m_vTargets;
	std::vector<std::string> m_vDisplayLabels;
	std::vector<SSettingsSearchHighlightRect> m_vMatches;
	std::vector<CUIRect> m_vDrawnRects;
	bool ObserveTarget(int TargetIndex, const CUIRect &Rect);

public:
	void SetTargets(std::vector<SSettingsSearchHighlightTarget> vTargets);
	void ClearMatches();
	int MatchLabel(std::string_view Text) const;
	int MatchConfig(const void *pConfig) const;
	bool ObserveLabel(std::string_view Text, const CUIRect &Rect);
	bool ObserveConfig(const void *pConfig, const CUIRect &Rect);
	bool TryMarkHighlightDrawn(const CUIRect &Rect);
	const std::vector<SSettingsSearchHighlightTarget> &Targets() const { return m_vTargets; }
	const std::vector<SSettingsSearchHighlightRect> &Matches() const { return m_vMatches; }
};

// 仅借用当前调用栈内的收集器，嵌套卡片或 nullptr 作用域结束后恢复外层。
class CUiScopedSettingsSearchHighlight
{
	CUi *m_pUi;
	CSettingsSearchHighlight *m_pPrevious = nullptr;

public:
	CUiScopedSettingsSearchHighlight(CUi *pUi, CSettingsSearchHighlight *pHighlight);
	~CUiScopedSettingsSearchHighlight();
	CUiScopedSettingsSearchHighlight(const CUiScopedSettingsSearchHighlight &) = delete;
	CUiScopedSettingsSearchHighlight &operator=(const CUiScopedSettingsSearchHighlight &) = delete;
};

#endif
