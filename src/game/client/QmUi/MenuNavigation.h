#ifndef GAME_CLIENT_QMUI_MENUNAVIGATION_H
#define GAME_CLIENT_QMUI_MENUNAVIGATION_H

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>

namespace qm_navigation
{
	struct SLayout
	{
		static constexpr int MAX_LEFT = 10;
		static constexpr int MAX_RIGHT = 6;
		std::array<CUIRect, MAX_LEFT> m_aLeft{};
		std::array<CUIRect, MAX_RIGHT> m_aRight{};
		int m_LeftCount = 0;
		int m_RightCount = 0;
		float m_ContentScale = 1.0f;
	};

	// 两组共用槽位尺寸；空间不足时一起收缩，社区数量变化不会挤占右组。
	inline SLayout Resolve(const CUIRect &Rect, int LeftCount, int RightCount, float BaseHeight, float Scale)
	{
		SLayout Result;
		Result.m_LeftCount = std::clamp(LeftCount, 0, SLayout::MAX_LEFT);
		Result.m_RightCount = std::clamp(RightCount, 0, SLayout::MAX_RIGHT);
		const int Count = Result.m_LeftCount + Result.m_RightCount;
		if(Count == 0)
			return Result;
		Scale = std::clamp(Scale, 0.75f, 1.5f);
		const float Width = std::max(0.0f, Rect.w);
		const float Height = std::min(std::max(0.0f, Rect.h), std::max(0.0f, BaseHeight) * Scale);
		const float BaseWidth = std::max(0.0f, BaseHeight) * 2.4f;
		const float GroupGap = Result.m_LeftCount > 0 && Result.m_RightCount > 0 ? std::min(12.0f * Scale, Width * 0.04f) : 0.0f;
		const int Gaps = std::max(0, Result.m_LeftCount - 1) + std::max(0, Result.m_RightCount - 1);
		const float ItemGap = std::min(4.0f * Scale, std::max(0.0f, Width - GroupGap) / (Count * 4.0f));
		const float Size = std::max(0.0f, std::min(BaseWidth * Scale, (Width - GroupGap - Gaps * ItemGap) / Count));
		Result.m_ContentScale = BaseHeight > 0.0f ? std::min(Height / BaseHeight, Size / BaseWidth) : 1.0f;
		for(int Index = 0; Index < Result.m_LeftCount; ++Index)
			Result.m_aLeft[Index] = {Rect.x + Index * (Size + ItemGap), Rect.y, Size, Height};
		const float RightWidth = Result.m_RightCount * Size + std::max(0, Result.m_RightCount - 1) * ItemGap;
		for(int Index = 0; Index < Result.m_RightCount; ++Index)
			Result.m_aRight[Index] = {Rect.x + Width - RightWidth + Index * (Size + ItemGap), Rect.y, Size, Height};
		return Result;
	}
}

#endif
