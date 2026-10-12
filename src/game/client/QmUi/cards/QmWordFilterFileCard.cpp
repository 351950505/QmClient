#include <engine/shared/config.h>

#include <game/client/QmUi/UiForms.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

void CMenus::RenderQmWordFilterFileContent(CUIRect &Content, bool Streamer, float LineHeight, float LineSpacing, float BodySize)
{
	static CLineInputBuffered<1024> s_aPathInputs[2];
	static CButtonContainer s_aReloadButtons[2];
	const int Index = Streamer ? 1 : 0;
	auto &Input = s_aPathInputs[Index];
	char *pPath = Streamer ? g_Config.m_QmStreamerWordsFile : g_Config.m_QmBlockWordsFile;
	if(!Input.IsActive() && str_comp(Input.GetString(), pPath) != 0)
		Input.Set(pPath);
	CUIRect Row;
	Content.HSplitTop(LineHeight, &Row, &Content);
	Ui()->DoLabel(&Row, Localize("Word list file"), BodySize, TEXTALIGN_ML);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	Content.HSplitTop(LineHeight, &Row, &Content);
	IUiContext Context = SettingsUiContext(Streamer ? "streamer_word_file" : "chat_word_file", CurrentSettingsContentMetrics().m_UiScale);
	if(ui_widget::InputField(Context, &Input, Row, "", BodySize))
	{
		str_copy(pPath, Input.GetString(), sizeof(g_Config.m_QmBlockWordsFile));
		GameClient()->m_Chat.ReloadWordFilterFile(Streamer, false);
	}
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	Content.HSplitTop(LineHeight, &Row, &Content);
	CUIRect Reload, Count;
	Row.VSplitLeft(std::min(Row.w * 0.35f, 100.0f), &Reload, &Count);
	if(DoButton_Menu(&s_aReloadButtons[Index], Localize("Reload"), 0, &Reload))
		GameClient()->m_Chat.ReloadWordFilterFile(Streamer);
	const auto &File = GameClient()->m_Chat.WordFilterFile(Streamer);
	char aCount[128];
	str_format(aCount, sizeof(aCount), Localize("%d words loaded"), static_cast<int>(File.Words().size()));
	Count.VMargin(5.0f, &Count);
	Ui()->DoLabel(&Count, aCount, BodySize, TEXTALIGN_ML);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	Content.HSplitTop(LineHeight * 2.0f + LineSpacing, &Row, &Content);
	SLabelProperties Properties;
	Properties.m_MaxWidth = Row.w;
	const char *pStatus = File.Error().empty() ? Localize("UTF-8 text, one word per line (max 2 MiB).") : Localize(File.Error().c_str());
	Ui()->DoLabel(&Row, pStatus, BodySize, TEXTALIGN_ML, Properties);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
}
