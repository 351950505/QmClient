#include "SettingsSearchHighlight.h"

#include "QmCardLabelHints.h"

#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	bool ValidHighlightRect(const CUIRect &Rect)
	{
		return std::isfinite(Rect.x) && std::isfinite(Rect.y) && std::isfinite(Rect.w) && std::isfinite(Rect.h) && Rect.w > 0.0f && Rect.h > 0.0f;
	}

	bool LabelSpace(char Character)
	{
		return Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r';
	}

	std::string_view TrimLabel(std::string_view Text)
	{
		while(!Text.empty() && LabelSpace(Text.front()))
			Text.remove_prefix(1);
		while(!Text.empty() && LabelSpace(Text.back()))
			Text.remove_suffix(1);
		return Text;
	}

	bool LabelWithValueMatches(std::string_view Text, std::string_view Label)
	{
		if(Label.empty() || Text.size() < Label.size() || Text.substr(0, Label.size()) != Label)
			return false;
		if(Text.size() == Label.size())
			return true;
		std::string_view Suffix = Text.substr(Label.size());
		if(Suffix.front() == ':')
			Suffix.remove_prefix(1);
		else if(Suffix.substr(0, 3) == "：")
			Suffix.remove_prefix(3);
		else if(!LabelSpace(Suffix.front()))
			return false;
		Suffix = TrimLabel(Suffix);
		if(!Suffix.empty() && (Suffix.front() == '+' || Suffix.front() == '-'))
			Suffix.remove_prefix(1);
		if(!Suffix.empty() && Suffix.front() == '.')
			Suffix.remove_prefix(1);
		return !Suffix.empty() && Suffix.front() >= '0' && Suffix.front() <= '9';
	}
}

void CSettingsSearchHighlight::SetTargets(std::vector<SSettingsSearchHighlightTarget> vTargets)
{
	m_vTargets = std::move(vTargets);
	m_vDisplayLabels.clear();
	m_vDisplayLabels.reserve(m_vTargets.size());
	for(SSettingsSearchHighlightTarget &Target : m_vTargets)
	{
		Target.m_Label = std::string(TrimLabel(Target.m_Label));
		const SQmCardLabelText Display = QmSplitCardLabel(Target.m_Label);
		m_vDisplayLabels.emplace_back(TrimLabel(Display.m_Label));
	}
	ClearMatches();
}

void CSettingsSearchHighlight::ClearMatches()
{
	m_vMatches.clear();
	m_vDrawnRects.clear();
}

int CSettingsSearchHighlight::MatchLabel(std::string_view Text) const
{
	Text = TrimLabel(Text);
	for(size_t Index = 0; Index < m_vTargets.size(); ++Index)
	{
		if(LabelWithValueMatches(Text, m_vTargets[Index].m_Label) || LabelWithValueMatches(Text, m_vDisplayLabels[Index]))
			return static_cast<int>(Index);
	}
	return -1;
}

bool CSettingsSearchHighlight::ObserveLabel(std::string_view Text, const CUIRect &Rect)
{
	return ObserveTarget(MatchLabel(Text), Rect);
}

int CSettingsSearchHighlight::MatchConfig(const void *pConfig) const
{
	if(pConfig == nullptr)
		return -1;
	for(size_t Index = 0; Index < m_vTargets.size(); ++Index)
		if(m_vTargets[Index].m_pConfig == pConfig)
			return static_cast<int>(Index);
	return -1;
}

bool CSettingsSearchHighlight::ObserveConfig(const void *pConfig, const CUIRect &Rect)
{
	return ObserveTarget(MatchConfig(pConfig), Rect);
}

bool CSettingsSearchHighlight::ObserveTarget(int TargetIndex, const CUIRect &Rect)
{
	if(!ValidHighlightRect(Rect))
		return false;
	if(TargetIndex < 0)
		return false;
	const auto Existing = std::find_if(m_vMatches.begin(), m_vMatches.end(), [TargetIndex, &Rect](const SSettingsSearchHighlightRect &Match) {
		return Match.m_TargetIndex == static_cast<size_t>(TargetIndex) && Match.m_Rect.x == Rect.x && Match.m_Rect.y == Rect.y && Match.m_Rect.w == Rect.w && Match.m_Rect.h == Rect.h;
	});
	if(Existing == m_vMatches.end())
		m_vMatches.push_back({static_cast<size_t>(TargetIndex), Rect});
	return true;
}

bool CSettingsSearchHighlight::TryMarkHighlightDrawn(const CUIRect &Rect)
{
	if(!ValidHighlightRect(Rect))
		return false;
	for(const CUIRect &Drawn : m_vDrawnRects)
	{
		// 配置入口先画整行时，后续标签不重复叠加；反向调用也不覆盖已画文字。
		if(Rect.x < Drawn.x + Drawn.w && Drawn.x < Rect.x + Rect.w && Rect.y < Drawn.y + Drawn.h && Drawn.y < Rect.y + Rect.h)
			return false;
	}
	m_vDrawnRects.push_back(Rect);
	return true;
}

CUiScopedSettingsSearchHighlight::CUiScopedSettingsSearchHighlight(CUi *pUi, CSettingsSearchHighlight *pHighlight) :
	m_pUi(pUi)
{
	if(m_pUi != nullptr)
	{
		m_pPrevious = m_pUi->m_pSettingsSearchHighlight;
		m_pUi->m_pSettingsSearchHighlight = pHighlight;
	}
}

CUiScopedSettingsSearchHighlight::~CUiScopedSettingsSearchHighlight()
{
	if(m_pUi != nullptr)
		m_pUi->m_pSettingsSearchHighlight = m_pPrevious;
}
