#ifndef GAME_CLIENT_QMUI_QMPIEMENUSETTINGSCONTROLS_H
#define GAME_CLIENT_QMUI_QMPIEMENUSETTINGSCONTROLS_H

#include "UiSurface.h"

#include <game/client/ui.h>

inline CUIRect QmPieOptionSwatchRect(const CUIRect &Cell, float LineHeight)
{
	const float Size = std::max(0.0f, std::min({LineHeight, Cell.w, Cell.h}));
	return {Cell.x + Cell.w - Size, Cell.y + (Cell.h - Size) * 0.5f, Size, Size};
}

inline CUIRect QmPieOptionButtonRect(CUIRect Cell, float LineHeight, float Spacing)
{
	Cell.w = std::max(0.0f, Cell.w - QmPieOptionSwatchRect(Cell, LineHeight).w - std::max(0.0f, Spacing));
	return Cell;
}

// 配色与文字按钮各自处理命中；后台保留色块绘制，预布局只处理输入。
inline bool QmDrawPieOptionColorControl(const IUiContext &Ctx, const void *pColorId, ColorRGBA Color, const CUIRect &Swatch, float CornerRadius, bool ProcessInput)
{
	if(Ctx.m_pUi == nullptr || (Ctx.m_pUi->DrawingSuppressed() && !Ctx.m_pUi->PreLayoutInput()))
		return false;
	DrawRoundedSurface(Ctx, Swatch, Color, ColorRGBA(), CornerRadius);
	return ProcessInput && !Ctx.m_pUi->RenderOnly() && Ctx.m_pUi->DoButtonLogic(pColorId, 0, &Swatch, BUTTONFLAG_LEFT) != 0;
}

#endif
