#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_HUD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_HUD_LAYOUT_H

#include <algorithm>

struct SQmSpectatorHudLayout
{
	float m_Left;
	float m_Width;
	float m_TextWidth;
};

// 右侧锚定，按文字与辅助标签的较宽者收缩；窄屏保留非负内容宽度。
inline SQmSpectatorHudLayout QmSpectatorHudLayout(float ScreenWidth, float TextWidth, float AuxiliaryWidth = 0.0f)
{
	constexpr float Padding = 6.0f;
	const float Width = std::min(std::max(0.0f, ScreenWidth), std::max(std::max(0.0f, TextWidth) + Padding * 2.0f, std::max(0.0f, AuxiliaryWidth)));
	return {std::max(0.0f, ScreenWidth - Width), Width, std::max(0.0f, Width - Padding * 2.0f)};
}

#endif
