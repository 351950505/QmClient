#ifndef GAME_CLIENT_QMUI_SETTINGSCARDMEASURE_H
#define GAME_CLIENT_QMUI_SETTINGSCARDMEASURE_H

#include "SettingsCard.h"

#include <game/client/ui.h>

// 后台卡片重测也必须隐藏探测外观，退出后恢复父层真实裁剪。
inline float MeasureSettingsCardContent(CUi *pUi, const FSettingsCardRenderMeasured &Render, float ContentWidth)
{
	if(!Render)
		return 0.0f;
	const CUiScopedMeasurement Measurement(pUi);
	return ResolveSettingsCardContentHeight(Render, ContentWidth);
}

#endif
