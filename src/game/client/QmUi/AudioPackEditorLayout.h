#ifndef GAME_CLIENT_QMUI_AUDIOPACKEDITORLAYOUT_H
#define GAME_CLIENT_QMUI_AUDIOPACKEDITORLAYOUT_H

#include <game/client/ui_rect.h>

#include <algorithm>

namespace qm_audio_pack
{
	struct SEditorLayout
	{
		CUIRect m_Catalog;
		CUIRect m_Workspace;
		bool m_Stacked;
	};

	inline SEditorLayout ResolveEditorLayout(CUIRect View, float UiScale)
	{
		SEditorLayout Layout{};
		View.w = std::max(0.0f, View.w);
		View.h = std::max(0.0f, View.h);
		const float Scale = std::max(0.1f, UiScale);
		Layout.m_Stacked = View.w < 520.0f * Scale;
		if(Layout.m_Stacked)
		{
			const float Gap = std::min(8.0f * Scale, View.h);
			View.HSplitTop((View.h - Gap) * 0.42f, &Layout.m_Catalog, &Layout.m_Workspace);
			Layout.m_Workspace.HSplitTop(Gap, nullptr, &Layout.m_Workspace);
		}
		else
		{
			const float Gap = std::min(10.0f * Scale, View.w);
			const float Width = std::min(280.0f * Scale, (View.w - Gap) * 0.30f);
			View.VSplitLeft(Width, &Layout.m_Catalog, &Layout.m_Workspace);
			Layout.m_Workspace.VSplitLeft(Gap, nullptr, &Layout.m_Workspace);
		}
		return Layout;
	}
}

#endif
