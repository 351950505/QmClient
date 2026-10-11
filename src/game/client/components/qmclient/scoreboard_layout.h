#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_LAYOUT_H

#include <engine/client.h>

#include <game/client/ui_rect.h>
#include <game/teamscore.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

struct SScoreboardTeamLabelLayout
{
	float m_X;
	float m_Y;
	float m_IconY;
	float m_RowSpacing;
};

constexpr float SCOREBOARD_TEAM_MODE_ICON_SIZE = 12.0f;

constexpr SScoreboardTeamLabelLayout ResolveScoreboardTeamLabelLayout(float RowX, float RowY, float RowHeight, float Spacing, float TeamFontSize, float TeamIconSize, bool EndsDDTeam)
{
	const float ContentHeight = TeamIconSize > TeamFontSize ? TeamIconSize : TeamFontSize;
	const float RowSpacing = EndsDDTeam && ContentHeight > Spacing ? ContentHeight : Spacing;
	const float ContentY = RowY + RowHeight;
	return {
		RowX + 5.0f,
		ContentY + (RowSpacing - TeamFontSize) / 2.0f,
		ContentY + (RowSpacing - TeamIconSize) / 2.0f,
		RowSpacing,
	};
}

constexpr float ScoreboardRowsHeight(int NumRows, int NumTeamLabels, int NumTeamModeLabels, float LineHeight, float Spacing, float TeamFontSize, float TeamModeIconSize)
{
	if(NumRows <= 0)
		return 0.0f;
	const int ClampedTeamLabels = NumTeamLabels < 0 ? 0 : (NumTeamLabels > NumRows ? NumRows : NumTeamLabels);
	const int ClampedTeamModeLabels = NumTeamModeLabels < 0 ? 0 : (NumTeamModeLabels > ClampedTeamLabels ? ClampedTeamLabels : NumTeamModeLabels);
	const float TextSpacing = ResolveScoreboardTeamLabelLayout(0.0f, 0.0f, LineHeight, Spacing, TeamFontSize, 0.0f, true).m_RowSpacing;
	const float IconSpacing = ResolveScoreboardTeamLabelLayout(0.0f, 0.0f, LineHeight, Spacing, TeamFontSize, TeamModeIconSize, true).m_RowSpacing;
	return NumRows * LineHeight + (NumRows - ClampedTeamLabels) * Spacing +
	       (ClampedTeamLabels - ClampedTeamModeLabels) * TextSpacing + ClampedTeamModeLabels * IconSpacing;
}

constexpr float ScoreboardRowsVerticalScale(float AvailableHeight, int NumRows, int NumTeamLabels, int NumTeamModeLabels, float LineHeight, float Spacing, float TeamFontSize, float TeamModeIconSize)
{
	if(NumRows <= 0)
		return 1.0f;
	if(AvailableHeight <= 0.0f)
		return 0.0f;
	const int ClampedTeamLabels = NumTeamLabels < 0 ? 0 : (NumTeamLabels > NumRows ? NumRows : NumTeamLabels);
	const int ClampedTeamModeLabels = NumTeamModeLabels < 0 ? 0 : (NumTeamModeLabels > ClampedTeamLabels ? ClampedTeamLabels : NumTeamModeLabels);
	const int TextOnlyTeamLabels = ClampedTeamLabels - ClampedTeamModeLabels;
	const int RowsWithoutTeamLabels = NumRows - ClampedTeamLabels;
	const float TeamTextSpacing = TeamFontSize > Spacing ? TeamFontSize : Spacing;
	const float TeamModeSpacing = TeamModeIconSize > TeamTextSpacing ? TeamModeIconSize : TeamTextSpacing;
	// 图标与文字随玩家行一起缩放，高度预算必须使用同一比例。
	const float RequiredHeight = NumRows * LineHeight + RowsWithoutTeamLabels * Spacing + TextOnlyTeamLabels * TeamTextSpacing + ClampedTeamModeLabels * TeamModeSpacing;
	if(RequiredHeight <= AvailableHeight)
		return 1.0f;
	return AvailableHeight / RequiredHeight;
}

constexpr CUIRect ScoreboardPlayerRowsRect(CUIRect Column, float HeadlineHeight)
{
	const float HeaderHeight = maximum(0.0f, minimum(HeadlineHeight, Column.h));
	return {Column.x, Column.y + HeaderHeight, Column.w, maximum(0.0f, Column.h - HeaderHeight - 5.0f)};
}

struct SQmScoreboardRowMetrics
{
	float m_LineHeight = 20.0f;
	float m_FontSize = 12.0f;
	float m_TeeSizeMod = 0.4f;
	float m_Spacing = 0.0f;
	float m_RoundRadius = 2.5f;
};

inline SQmScoreboardRowMetrics QmScoreboardPreferredRowMetrics(int NumRows, bool Scroll)
{
	SQmScoreboardRowMetrics Metrics;
	if(!Scroll && NumRows <= 8)
		Metrics = {30.0f, 12.0f, 0.5f, 8.0f, 5.0f};
	else if(!Scroll && NumRows <= 12)
		Metrics = {25.0f, 12.0f, 0.45f, 2.5f, 5.0f};
	else if(!Scroll && NumRows > 16)
		Metrics = {16.0f, 12.0f, 0.25f, 0.0f, 2.5f};
	return Metrics;
}

inline SQmScoreboardRowMetrics QmScoreboardRowMetrics(float AvailableHeight, int NumRows, int NumTeamLabels, int NumTeamModeLabels, bool Scroll)
{
	SQmScoreboardRowMetrics Metrics = QmScoreboardPreferredRowMetrics(NumRows, Scroll);
	const float RequiredHeight = ScoreboardRowsHeight(NumRows, NumTeamLabels, NumTeamModeLabels, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, minimum(SCOREBOARD_TEAM_MODE_ICON_SIZE, Metrics.m_FontSize));
	const float FitScale = RequiredHeight > 0.0f ? maximum(0.0f, AvailableHeight) / RequiredHeight : 1.0f;
	const float Scale = Scroll ? 1.0f : minimum(NumRows > 16 ? 1.25f : 1.0f, FitScale);
	Metrics.m_LineHeight *= Scale;
	Metrics.m_FontSize *= Scale;
	Metrics.m_TeeSizeMod *= Scale;
	Metrics.m_Spacing *= Scale;
	Metrics.m_RoundRadius *= Scale;
	// 多人列利用新增纵向空间；字号适度放大后将剩余高度均匀分配给玩家行。
	if(!Scroll && NumRows > 16)
	{
		const float UsedHeight = ScoreboardRowsHeight(NumRows, NumTeamLabels, NumTeamModeLabels, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, minimum(SCOREBOARD_TEAM_MODE_ICON_SIZE, Metrics.m_FontSize));
		Metrics.m_LineHeight += maximum(0.0f, AvailableHeight - UsedHeight) / NumRows;
	}
	return Metrics;
}

struct SQmScoreboardLayoutRow
{
	int m_DDTeam = TEAM_FLOCK;
	bool m_TeamLabel = false;
	bool m_TeamModeLabel = false;
};

struct SQmScoreboardColumns
{
	static constexpr int MAX_COLUMNS = 3;
	static constexpr float MIN_FONT_SIZE = 10.0f;
	int m_Count = 1;
	std::array<int, MAX_COLUMNS> m_aEndRows{};
	bool m_Scroll = false;
};

// 普通队伍保持完整；超过单列可读预算的大队伍允许连续跨列，不移动玩家。
inline SQmScoreboardColumns QmScoreboardColumns(const std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> &aRows, int NumRows,
	float AvailableWidth, float AvailableHeight, float MinimumColumnWidth)
{
	NumRows = std::clamp<int>(NumRows, 0, MAX_CLIENTS);
	SQmScoreboardColumns Result;
	Result.m_aEndRows[0] = NumRows;
	if(NumRows == 0)
		return Result;
	const int MaxColumns = std::clamp(static_cast<int>(AvailableWidth / maximum(1.0f, MinimumColumnWidth)), 1, SQmScoreboardColumns::MAX_COLUMNS);

	std::array<int, MAX_CLIENTS + 1> aLabels{};
	std::array<int, MAX_CLIENTS + 1> aModeLabels{};
	std::array<bool, MAX_CLIENTS + 1> aBoundary{};
	aBoundary[0] = true;
	for(int i = 0; i < NumRows; ++i)
	{
		aLabels[i + 1] = aLabels[i] + aRows[i].m_TeamLabel;
		aModeLabels[i + 1] = aModeLabels[i] + (aRows[i].m_TeamLabel && aRows[i].m_TeamModeLabel);
		aBoundary[i + 1] = i + 1 == NumRows || aRows[i].m_DDTeam == TEAM_FLOCK || aRows[i].m_DDTeam != aRows[i + 1].m_DDTeam;
	}
	for(int Start = 0; Start < NumRows;)
	{
		int End = Start + 1;
		while(End < NumRows && aRows[End].m_DDTeam == aRows[Start].m_DDTeam)
			++End;
		const int Labels = aLabels[End] - aLabels[Start];
		const int ModeLabels = aModeLabels[End] - aModeLabels[Start];
		const auto Metrics = QmScoreboardPreferredRowMetrics(End - Start, false);
		const float Height = ScoreboardRowsHeight(End - Start, Labels, ModeLabels, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, Metrics.m_FontSize);
		if(Height > maximum(0.0f, AvailableHeight) * 12.0f / SQmScoreboardColumns::MIN_FONT_SIZE)
			for(int Boundary = Start + 1; Boundary < End; ++Boundary)
				aBoundary[Boundary] = true;
		Start = End;
	}

	// 最小化最高一列的真实高度，队伍标签和模式图标与玩家行共同占预算。
	std::array<std::array<float, MAX_CLIENTS + 1>, SQmScoreboardColumns::MAX_COLUMNS + 1> aCosts;
	std::array<std::array<int, MAX_CLIENTS + 1>, SQmScoreboardColumns::MAX_COLUMNS + 1> aStarts{};
	for(auto &aCost : aCosts)
		aCost.fill(std::numeric_limits<float>::max());
	aCosts[0][0] = 0.0f;
	float BestHeight = std::numeric_limits<float>::max();
	for(int Columns = 1; Columns <= MaxColumns; ++Columns)
	{
		for(int End = 1; End <= NumRows; ++End)
		{
			if(!aBoundary[End])
				continue;
			for(int Start = 0; Start < End; ++Start)
			{
				if(!aBoundary[Start] || aCosts[Columns - 1][Start] == std::numeric_limits<float>::max())
					continue;
				const int Labels = aLabels[End] - aLabels[Start];
				const int ModeLabels = aModeLabels[End] - aModeLabels[Start];
				const SQmScoreboardRowMetrics Metrics = QmScoreboardPreferredRowMetrics(End - Start, false);
				const float Height = ScoreboardRowsHeight(End - Start, Labels, ModeLabels, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, Metrics.m_FontSize);
				const float Cost = maximum(aCosts[Columns - 1][Start], Height);
				if(Cost < aCosts[Columns][End])
				{
					aCosts[Columns][End] = Cost;
					aStarts[Columns][End] = Start;
				}
			}
		}
		if(aCosts[Columns][NumRows] < BestHeight)
		{
			BestHeight = aCosts[Columns][NumRows];
			Result.m_Count = Columns;
		}
		// 文字能保持首选字号时优先少列，为长名字和战队名保留横向空间。
		if(BestHeight <= AvailableHeight)
			break;
	}
	int End = NumRows;
	for(int Column = Result.m_Count - 1; Column >= 0; --Column)
	{
		Result.m_aEndRows[Column] = End;
		End = aStarts[Column + 1][End];
	}
	// 所有列仍无法保持可读文字时交给连续滚动，不能只放大字号溢出。
	if(BestHeight > maximum(0.0f, AvailableHeight) * 12.0f / SQmScoreboardColumns::MIN_FONT_SIZE)
	{
		Result = {};
		Result.m_aEndRows[0] = NumRows;
		Result.m_Scroll = true;
	}
	return Result;
}

// 排序内容无需进入缓存键；只有队伍边界、标签占位及视口预算影响分列。
class CQmScoreboardLayoutCache
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> m_aRows{};
	int m_NumRows = -1;
	float m_Width = 0.0f;
	float m_Height = 0.0f;
	float m_MinimumColumnWidth = 0.0f;
	SQmScoreboardColumns m_Columns;
	uint64_t m_Revision = 0;

public:
	const SQmScoreboardColumns &Resolve(const std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> &aRows, int NumRows,
		float Width, float Height, float MinimumColumnWidth)
	{
		NumRows = std::clamp<int>(NumRows, 0, MAX_CLIENTS);
		bool Changed = NumRows != m_NumRows || Width != m_Width || Height != m_Height || MinimumColumnWidth != m_MinimumColumnWidth;
		for(int i = 0; !Changed && i < NumRows; ++i)
			Changed = aRows[i].m_DDTeam != m_aRows[i].m_DDTeam || aRows[i].m_TeamLabel != m_aRows[i].m_TeamLabel || aRows[i].m_TeamModeLabel != m_aRows[i].m_TeamModeLabel;
		if(Changed)
		{
			m_aRows = aRows;
			m_NumRows = NumRows;
			m_Width = Width;
			m_Height = Height;
			m_MinimumColumnWidth = MinimumColumnWidth;
			m_Columns = QmScoreboardColumns(aRows, NumRows, Width, Height, MinimumColumnWidth);
			++m_Revision;
		}
		return m_Columns;
	}

	uint64_t Revision() const { return m_Revision; }
};

struct SQmScoreboardIdentityWidths
{
	float m_Name;
	float m_Clan;
};

inline bool QmScoreboardToggleCursor(bool &Unlocked, bool Active, bool MenuActive, int ClientState)
{
	if(!Active || MenuActive || (ClientState != IClient::STATE_ONLINE && ClientState != IClient::STATE_DEMOPLAYBACK))
		return false;
	Unlocked = !Unlocked;
	return true;
}

inline bool QmScoreboardCursorInputEnabled(bool Active, bool Unlocked, int ClientState)
{
	return Active && Unlocked && (ClientState == IClient::STATE_ONLINE || ClientState == IClient::STATE_DEMOPLAYBACK);
}

enum class EQmScoreboardScrollMode
{
	FIT,
	AUTO,
	MANUAL
};

inline EQmScoreboardScrollMode QmScoreboardScrollMode(bool ManualEnabled, bool LayoutRequiresScroll)
{
	return ManualEnabled ? EQmScoreboardScrollMode::MANUAL : LayoutRequiresScroll ? EQmScoreboardScrollMode::AUTO :
											EQmScoreboardScrollMode::FIT;
}

inline int QmScoreboardManualScrollAfterClick(EQmScoreboardScrollMode Mode)
{
	// 自动布局点击后固定为手动滚动；关闭手动模式后恢复空间驱动的自动布局。
	return Mode == EQmScoreboardScrollMode::MANUAL ? 0 : 1;
}

inline SQmScoreboardIdentityWidths QmScoreboardIdentityWidths(float AvailableWidth, bool ShowClan)
{
	const float Width = maximum(0.0f, AvailableWidth);
	const float NameWidth = ShowClan ? Width * 0.6f : Width;
	return {NameWidth, ShowClan ? Width - NameWidth : 0.0f};
}

#endif
