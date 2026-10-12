#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/QmUi/SettingsContentLayout.h>
#include <game/client/QmUi/UiForms.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

void CMenus::RenderQmWordFilterFileContent(CUIRect &Content, bool Streamer, float LineHeight, float LineSpacing, float BodySize)
{
	CSettingsContentLayoutFlow Flow(Content);
	static CLineInputBuffered<1024> s_aPathInputs[2];
	static CButtonContainer s_aReloadButtons[2];
	const int Index = Streamer ? 1 : 0;
	auto &Input = s_aPathInputs[Index];
	char *pPath = Streamer ? g_Config.m_QmStreamerWordsFile : g_Config.m_QmBlockWordsFile;
	if(Flow.Input() && !Ui()->RenderOnly() && !Input.IsActive() && str_comp(Input.GetString(), pPath) != 0)
		Input.Set(pPath);
	CUIRect Row;
	if(Flow.Control("word-filter-file-label", LineHeight, Row))
		Ui()->DoLabel(&Row, Localize("Word list file"), BodySize, TEXTALIGN_ML);
	Flow.Space(LineSpacing);
	IUiContext Context = SettingsUiContext(Streamer ? "streamer_word_file" : "chat_word_file", CurrentSettingsContentMetrics().m_UiScale);
	if(Flow.Control("word-filter-file-path", LineHeight, Row) && ui_widget::InputField(Context, &Input, Row, "", BodySize) && Flow.Input() && !Ui()->RenderOnly())
	{
		str_copy(pPath, Input.GetString(), sizeof(g_Config.m_QmBlockWordsFile));
		GameClient()->m_Chat.ReloadWordFilterFile(Streamer, false);
	}
	Flow.Space(LineSpacing);
	if(Flow.Control("word-filter-file-reload", LineHeight, Row))
	{
		CUIRect Reload, Count;
		Row.VSplitLeft(std::min(Row.w * 0.35f, 100.0f), &Reload, &Count);
		if(DoButton_Menu(&s_aReloadButtons[Index], Localize("Reload"), 0, &Reload) && Flow.Input() && !Ui()->RenderOnly())
			GameClient()->m_Chat.ReloadWordFilterFile(Streamer);
		const auto &File = GameClient()->m_Chat.WordFilterFile(Streamer);
		char aCount[128];
		str_format(aCount, sizeof(aCount), Localize("%d words loaded"), static_cast<int>(File.Words().size()));
		Count.VMargin(5.0f, &Count);
		Ui()->DoLabel(&Count, aCount, BodySize, TEXTALIGN_ML);
	}
	Flow.Space(LineSpacing);
	const auto &File = GameClient()->m_Chat.WordFilterFile(Streamer);
	const char *pStatus = File.Error().empty() ? Localize("UTF-8 text, one word per line (max 2 MiB).") : Localize(File.Error().c_str());
	Flow.Text("word-filter-file-status", pStatus, 0.0f, BodySize, LineSpacing, [this](const char *pText, float Width, float Size) { return TextRender()->TextBoundingBox(Size, pText, -1, Width).m_H; }, [this, BodySize](const CUIRect &Rect, const char *pText) { Ui()->DoLabel(&Rect, pText, BodySize, TEXTALIGN_ML, {.m_MaxWidth = Rect.w}); }, LineHeight);
}
