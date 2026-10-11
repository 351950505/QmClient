#ifndef GAME_CLIENT_QMUI_QMPIEMENUSETTINGSCONTROLS_H
#define GAME_CLIENT_QMUI_QMPIEMENUSETTINGSCONTROLS_H

#include "UiForms.h"
#include "UiSurface.h"

#include <game/client/ui.h>

struct SQmPieOptionControlsResult
{
	bool m_Enabled;
	bool m_EnabledChanged;
	bool m_ColorClicked;
	CUIRect m_Swatch;
};

// 饼菜单每个选项的开关和色块共用入口；后台保留绘制，测量与输入分别隔离。
inline SQmPieOptionControlsResult QmDrawPieOptionControls(const IUiContext &Ctx, const void *pToggleId, const void *pColorId, bool Enabled, ColorRGBA Color, const CUIRect &Cell, float LineHeight, float Spacing, float CornerRadius, bool ProcessInput)
{
	const float SwatchSize = std::min(LineHeight, Cell.w);
	const float Gap = std::min(Spacing, std::max(0.0f, Cell.w - SwatchSize));
	const float ToggleWidth = std::min(LineHeight * 2.0f, std::max(0.0f, Cell.w - SwatchSize - Gap));
	const float GroupWidth = ToggleWidth + Gap + SwatchSize;
	const CUIRect Toggle{Cell.x + (Cell.w - GroupWidth) * 0.5f, Cell.y, ToggleWidth, LineHeight};
	const CUIRect Swatch{Toggle.x + ToggleWidth + Gap, Cell.y, SwatchSize, LineHeight};
	SQmPieOptionControlsResult Result{Enabled, false, false, Swatch};
	if(Ctx.m_pUi == nullptr || Ctx.m_pUi->MeasurementOnly())
		return Result;
	Result.m_EnabledChanged = ui_widget::Toggle(Ctx, pToggleId, &Result.m_Enabled, Toggle, ProcessInput);
	DrawRoundedSurface(Ctx, Swatch, Color, ColorRGBA(), CornerRadius);
	Result.m_ColorClicked = ProcessInput && !Ctx.m_pUi->RenderOnly() && Ctx.m_pUi->DoButtonLogic(pColorId, 0, &Swatch, BUTTONFLAG_LEFT) != 0;
	return Result;
}

#endif
