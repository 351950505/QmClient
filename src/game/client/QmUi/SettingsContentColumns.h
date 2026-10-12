#ifndef GAME_CLIENT_QMUI_SETTINGSCONTENTCOLUMNS_H
#define GAME_CLIENT_QMUI_SETTINGSCONTENTCOLUMNS_H

#include "SettingsContentLayout.h"

struct SSettingsContentColumnsState
{
	std::array<CSettingsContentLayoutCache, 2> m_aCaches;
	std::array<std::shared_ptr<const CSettingsContentLayout>, 2> m_apLayouts;
};

// 二维编辑器左右列分别声明与测量；窄窗口按既有策略纵向排列，输入与绘制共用列快照。
inline void RenderSettingsContentColumns(const char *pId, CUIRect &Content, SSettingsContentColumnsState &State,
	float LeftWidth, float Gap, bool Stacked, uint64_t FontRevision,
	const CSettingsContentLayoutCache::FMeasureText &MeasureText,
	const std::array<std::function<void(CUIRect &)>, 2> &aRender)
{
	CSettingsContentLayoutFlow Flow(Content);
	const float Width = std::max(0.0f, Content.w);
	LeftWidth = std::clamp(LeftWidth, 0.0f, Width);
	Gap = std::clamp(Gap, 0.0f, Stacked ? Width : Width - LeftWidth);
	const float aWidths[] = {Stacked ? Width : LeftWidth, Stacked ? Width : Width - LeftWidth - Gap};
	if(Flow.Declaring() || !CSettingsContentLayoutScope::Current())
	{
		for(int Column = 0; Column < 2; ++Column)
		{
			std::vector<SSettingsContentRowDeclaration> vRows;
			CUIRect Region{0, 0, aWidths[Column], 0};
			{
				CSettingsContentLayoutScope Scope(vRows);
				aRender[Column](Region);
			}
			State.m_apLayouts[Column] = State.m_aCaches[Column].Resolve(vRows, aWidths[Column], FontRevision, MeasureText);
		}
	}
	if(!State.m_apLayouts[0] || !State.m_apLayouts[1])
		return;
	const float LeftHeight = State.m_apLayouts[0]->Height();
	const float RightHeight = State.m_apLayouts[1]->Height();
	CUIRect Block;
	if(!Flow.Control(pId, Stacked ? LeftHeight + Gap + RightHeight : std::max(LeftHeight, RightHeight), Block))
		return;
	const auto Pass = Flow.Input() ? ESettingsContentPass::INPUT : ESettingsContentPass::DRAW;
	for(int Column = 0; Column < 2; ++Column)
	{
		CUIRect Region{Block.x, Block.y, aWidths[Column], State.m_apLayouts[Column]->Height()};
		if(Column == 1)
		{
			if(Stacked)
				Region.y += LeftHeight + Gap;
			else
				Region.x += LeftWidth + Gap;
		}
		CSettingsContentLayoutScope Scope(*State.m_apLayouts[Column], Pass);
		aRender[Column](Region);
	}
}

#endif
