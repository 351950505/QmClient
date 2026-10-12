#ifndef GAME_CLIENT_QMUI_SETTINGSSEARCHSESSION_H
#define GAME_CLIENT_QMUI_SETTINGSSEARCHSESSION_H

#include <game/client/lineinput.h>
#include <game/client/ui.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// 搜索会话只维护稳定卡片身份；配置值变化不会改变结果选择与显露请求。
class CSettingsSearchSession
{
	std::string m_Query;
	std::vector<std::string> m_vResults;
	std::string m_Selected;
	std::string m_Reveal;

public:
	void SetResults(std::string Query, std::vector<std::string> vResults)
	{
		const bool QueryChanged = m_Query != Query;
		m_Query = std::move(Query);
		m_vResults = std::move(vResults);
		if(QueryChanged || std::find(m_vResults.begin(), m_vResults.end(), m_Selected) == m_vResults.end())
		{
			m_Selected = m_vResults.empty() ? "" : m_vResults.front();
			m_Reveal.clear();
		}
	}

	bool MoveSelection(int Direction, bool Composing = false, bool Blocked = false)
	{
		if(Composing || Blocked || m_vResults.empty() || Direction == 0)
			return false;
		const auto Current = std::find(m_vResults.begin(), m_vResults.end(), m_Selected);
		const int Index = Current == m_vResults.end() ? 0 : static_cast<int>(Current - m_vResults.begin());
		m_Selected = m_vResults[std::clamp(Index + (Direction > 0 ? 1 : -1), 0, static_cast<int>(m_vResults.size()) - 1)];
		m_Reveal = m_Selected;
		return true;
	}

	bool RevealSelection(bool Composing = false, bool Blocked = false)
	{
		if(Composing || Blocked || m_Selected.empty())
			return false;
		m_Reveal = m_Selected;
		return true;
	}

	const std::string &Query() const { return m_Query; }
	const std::string &Selected() const { return m_Selected; }
	const std::string &PendingReveal() const { return m_Reveal; }
	void FinishReveal() { m_Reveal.clear(); }
	void Clear() { *this = {}; }
};

// 离页和菜单关闭只释放编辑权；查询由输入控件显式清空，结果缓存沿用查询。
inline void ReleaseSettingsSearchInput(CUi *pUi, CLineInput *pInput)
{
	if(pUi->ActiveItem() == pInput)
		pUi->ReleaseActiveTextInput(pInput);
	else
		pInput->Deactivate();
}

#endif
