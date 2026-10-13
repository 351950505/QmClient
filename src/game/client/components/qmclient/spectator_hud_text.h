#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_HUD_TEXT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_HUD_TEXT_H

#include <game/client/QmUi/QmCardLabelHints.h>
#include <game/client/ui.h>

inline CLabelResult QmDrawSpectatorHudLabel(CUi &Ui, const CUIRect &Label, const char *pText, float MeasuredWidth)
{
	if(Label.w <= 0.0f || Label.h <= 0.0f)
		return {};

	SLabelProperties Props;
	Props.m_EnableWidthCheck = false;
	// 能放下时沿用测量时的无界字形规则，避免边距差和浮点舍入误触发省略号。
	if(MeasuredWidth > Label.w + 0.001f)
	{
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
	}
	const CQmCardLabelHintScope NameHints(&Ui, false);
	return Ui.DoLabel(&Label, pText, 8.0f, TEXTALIGN_ML, Props);
}

#endif
