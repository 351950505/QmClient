// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/QmUi/QmLayout.h>
#include <game/client/QmUi/UiSurfaceText.h>
#include <game/client/QmUi/UiTheme.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menus.h>
#include <game/client/components/qmclient/afk_presentation.h>
#include <game/client/components/qmclient/input_overlay.h>
#include <game/client/components/qmclient/score_hud_layout.h>
#include <game/client/components/qmclient/scoreboard_scroll.h>
#include <game/client/components/qmclient/scoreboard_team_modes.h>
#include <game/client/components/scoreboard.h>
#include <game/client/qm_icon.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <array>
#include <string>
#include <vector>

TEST(QmTuneColorMapper, NonArrayBackendsKeepTheOriginalTuneTileIndex)
{
	CTuneColorMapper Mapper;
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 7, false), TILE_TUNE);
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 0, true), TILE_TUNE);
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 7, true), 1);
}

TEST(QmCountryFlags, InvalidNetworkCountryCodesUseTheDefaultFlag)
{
	EXPECT_EQ(QmNormalizeCountryCode(0x74736554), CountryCode::DEFAULT);
	EXPECT_EQ(QmNormalizeCountryCode(-2), CountryCode::DEFAULT);
	EXPECT_EQ(QmNormalizeCountryCode(CountryCode::DEFAULT), CountryCode::DEFAULT);
	EXPECT_EQ(QmNormalizeCountryCode(156), 156);
}

TEST(QmCountryFlags, EntranceAnimationCalculatesExpectedScaleAndOvershoot)
{
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryScale(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryScale(-0.1f), 0.0f);

	const float MidScale = ComputeCountryFlagEntryScale(0.5185f);
	EXPECT_GE(MidScale, 1.19f);
	EXPECT_LE(MidScale, 1.21f);

	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryScale(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryScale(1.5f), 1.0f);

	const float ReducedMid = ComputeCountryFlagEntryScale(0.52f, 1.4f);
	EXPECT_GE(ReducedMid, 1.05f);
	EXPECT_LE(ReducedMid, 1.10f);
}

TEST(QmCountryFlags, EntranceAnimationCalculatesAlphaFadeIn)
{
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryAlpha(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryAlpha(-0.5f), 0.0f);
	EXPECT_NEAR(ComputeCountryFlagEntryAlpha(0.125f), 0.5f, 0.001f);
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryAlpha(0.25f), 1.0f);
	EXPECT_FLOAT_EQ(ComputeCountryFlagEntryAlpha(1.0f), 1.0f);
}

TEST(QmCountryFlags, EntranceAnimationPreservesCenterAnchorDuringScaling)
{
	const float X = 100.0f;
	const float Y = 50.0f;
	const float W = 64.0f;
	const float H = 32.0f;
	const float ExpectedCenterX = X + W * 0.5f;
	const float ExpectedCenterY = Y + H * 0.5f;

	for(float Scale : {0.0f, 0.5f, 1.0f, 1.20f, 1.5f})
	{
		float OutX = 0.0f, OutY = 0.0f, OutW = 0.0f, OutH = 0.0f;
		ComputeCountryFlagEntryRect(X, Y, W, H, Scale, OutX, OutY, OutW, OutH);

		EXPECT_FLOAT_EQ(OutW, W * Scale);
		EXPECT_FLOAT_EQ(OutH, H * Scale);
		EXPECT_NEAR(OutX + OutW * 0.5f, ExpectedCenterX, 0.001f);
		EXPECT_NEAR(OutY + OutH * 0.5f, ExpectedCenterY, 0.001f);
	}
}

TEST(QmAfkPresentation, ServerAndEscMenuStatesRemainAvailableForNonOpacityIndicators)
{
	EXPECT_TRUE(IsQmAfkForPresentation(true, false, false, 7, 3));
	EXPECT_TRUE(IsQmAfkForPresentation(false, true, true, 3, 3));

	EXPECT_FALSE(IsQmAfkForPresentation(false, true, false, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, 4, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, false, true, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, -1, -1));
}

TEST(QmScoreboardTeamModes, AggregationRequiresDisplayInfoAndCombinesKnownMembers)
{
	SQmScoreboardTeamModeState State;
	AccumulateQmScoreboardTeamModeState(State, false, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_FALSE(State.m_Known);
	EXPECT_EQ(State.m_Flags, 0);
	SQmScoreboardTeamModeState KnownEmptyState;
	AccumulateQmScoreboardTeamModeState(KnownEmptyState, true, 0);
	EXPECT_TRUE(KnownEmptyState.m_Known);
	EXPECT_EQ(KnownEmptyState.m_Flags, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_SOLO);
	EXPECT_TRUE(State.m_Known);
	EXPECT_TRUE(State.Practice());
	EXPECT_FALSE(State.Team0Mode());
	EXPECT_FALSE(State.Locked());
	EXPECT_EQ(State.m_Flags & CHARACTERFLAG_SOLO, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_TEAM0_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_TRUE(State.Practice());
	EXPECT_TRUE(State.Team0Mode());
	EXPECT_TRUE(State.Locked());
}

TEST(QmScoreboardRender, TitleTimeUsesTheSharedContentAlpha)
{
	const ColorRGBA Color = ScoreboardTitleTimeColor(0.35f);
	EXPECT_FLOAT_EQ(Color.r, 1.0f);
	EXPECT_FLOAT_EQ(Color.g, 1.0f);
	EXPECT_FLOAT_EQ(Color.b, 1.0f);
	EXPECT_FLOAT_EQ(Color.a, 0.35f);
	EXPECT_FLOAT_EQ(ScoreboardTitleTimeColor(0.0f).a, 0.0f);
	EXPECT_FLOAT_EQ(ScoreboardTitleTimeColor(1.0f).a, 1.0f);
}

TEST(QmScoreboardScroll, ModeRequiresCrowdedNonTeamGames)
{
	EXPECT_FALSE(ScoreboardScrollModeEnabled(true, true, 32));
	EXPECT_FALSE(ScoreboardScrollModeEnabled(false, false, 32));
	EXPECT_FALSE(ScoreboardScrollModeEnabled(false, true, 16));
	EXPECT_TRUE(ScoreboardScrollModeEnabled(false, true, 17));
}

TEST(QmScoreboardScroll, ScrollbarDisappearsWhenInteractionCloses)
{
	EXPECT_TRUE(ScoreboardScrollbarVisible(true, true, true, 1));
	EXPECT_FALSE(ScoreboardScrollbarVisible(true, false, true, 1));
	EXPECT_FALSE(ScoreboardScrollbarVisible(true, true, false, 1));
	EXPECT_FALSE(ScoreboardScrollbarVisible(true, true, true, 0));
}

TEST(QmScoreboardTeamModes, SpecPlayersKeepTheirScoreboardTeamAndLastKnownModeState)
{
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_GAME, false, false), TEAM_GAME);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, true, false), TEAM_GAME);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, false, false), TEAM_SPECTATORS);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, true, true), TEAM_SPECTATORS);

	constexpr int DdTeam = 3;
	std::array<SQmScoreboardTeamModeState, NUM_DDRACE_TEAMS> aTeamModes{};
	std::array<SQmScoreboardTeamModeState, NUM_DDRACE_TEAMS> aCachedTeamModes{};
	std::array<bool, NUM_DDRACE_TEAMS> aTeamHasPlayer{};

	aTeamModes[DdTeam].m_Known = true;
	aTeamModes[DdTeam].m_Flags = CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_LOCK_MODE;
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasPlayer, aCachedTeamModes);
	EXPECT_TRUE(aCachedTeamModes[DdTeam].Practice());
	EXPECT_TRUE(aCachedTeamModes[DdTeam].Locked());

	aTeamModes = {};
	aTeamHasPlayer[DdTeam] = true;
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasPlayer, aCachedTeamModes);
	EXPECT_TRUE(aTeamModes[DdTeam].m_Known);
	EXPECT_TRUE(aTeamModes[DdTeam].Practice());
	EXPECT_TRUE(aTeamModes[DdTeam].Locked());

	aTeamModes = {};
	aTeamHasPlayer = {};
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasPlayer, aCachedTeamModes);
	EXPECT_FALSE(aTeamModes[DdTeam].m_Known);
}

TEST(UiV2Layout, RowPaddingGapAndPosition)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 5.0f;
	ContainerStyle.m_Padding = {10.0f, 10.0f, 10.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::START;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 200.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 20.0f);

	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 65.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_H, 20.0f);
}

TEST(UiV2Layout, RowFlexDistribution)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 10.0f;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 230.0f, 40.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(2.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Px(30.0f);
	vChildren[2].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 60.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 120.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_W, 30.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 200.0f);
}

TEST(UiV2Layout, ColumnJustifyCenter)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 10.0f;
	ContainerStyle.m_JustifyContent = EUiAlign::CENTER;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 80.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 25.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 55.0f);
}

TEST(UiV2Layout, AlignStretchExpandsCrossAxis)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Padding = {0.0f, 10.0f, 0.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 100.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(1);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Auto();

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 80.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
}

TEST(UiV2Layout, ApplyConstraintsMinMaxPercent)
{
	CUiV2LayoutEngine Engine;
	SUiStyle Style;
	Style.m_Width = SUiLength::Percent(0.5f);
	Style.m_Height = SUiLength::Px(100.0f);
	Style.m_MinWidth = SUiLength::Px(120.0f);
	Style.m_MaxWidth = SUiLength::Px(180.0f);
	Style.m_MaxHeight = SUiLength::Px(70.0f);

	SUiLayoutBox Parent{0.0f, 0.0f, 300.0f, 300.0f};
	const SUiLayoutBox Box = Engine.ApplyConstraints(Style, Parent);

	EXPECT_FLOAT_EQ(Box.m_W, 150.0f);
	EXPECT_FLOAT_EQ(Box.m_H, 70.0f);
}

TEST(UiV2Layout, ScoreboardTeamColumnsWithGap)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 7.5f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 850.0f, 385.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 421.25f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 428.75f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 421.25f);
}

TEST(UiV2Layout, ScoreboardThreeColumnsEqualWidth)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 900.0f, 320.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 600.0f);
}

TEST(UiV2Layout, ScoreboardSoundMuteVerticalButtons)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 4.0f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 22.0f, 230.0f};

	std::vector<SUiLayoutChild> vChildren(9);
	for(SUiLayoutChild &Child : vChildren)
	{
		Child.m_Style.m_Height = SUiLength::Px(22.0f);
	}

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 22.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[8].m_Box.m_Y, 208.0f);
}

TEST(QmGaussianBlurRender, TargetUsesQuarterResolutionAndRoundsUp)
{
	EXPECT_EQ(UiGaussianBlurTargetDimension(1920), 480);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1080), 270);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1081), 271);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1), 1);
	EXPECT_EQ(UiGaussianBlurTargetDimension(0), 0);
}

TEST(QmScoreboardRender, PlayerRowsAlwaysUseFullDetail)
{
	const SScoreboardRowRenderDetail Detail = ResolveScoreboardRowRenderDetail();
	EXPECT_TRUE(Detail.m_FullTee);
	EXPECT_TRUE(Detail.m_ShowClientBrand);
	EXPECT_TRUE(Detail.m_ShowClan);
	EXPECT_TRUE(Detail.m_ShowCountry);
}

TEST(QmScoreboardRender, DdTeamLabelUsesBelowRowLayoutRegardlessOfColumnCount)
{
	const SScoreboardTeamLabelLayout SingleColumn = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 8.0f, 8.0f, 0.0f, true);
	const SScoreboardTeamLabelLayout MultiColumn = ResolveScoreboardTeamLabelLayout(220.0f, 40.0f, 30.0f, 2.5f, 8.0f, 0.0f, true);

	EXPECT_FLOAT_EQ(SingleColumn.m_X, 25.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_X, 225.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_RowSpacing, 8.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_RowSpacing, 8.0f);

	// A DDTeam that continues in the next column must not reserve or render a duplicate label.
	const SScoreboardTeamLabelLayout ContinuedTeam = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 2.5f, 8.0f, SCOREBOARD_TEAM_MODE_ICON_SIZE, false);
	EXPECT_FLOAT_EQ(ContinuedTeam.m_RowSpacing, 2.5f);
}

TEST(QmScoreboardRender, DdTeamModeIconsUseNativeHudSizeAndCenteredLabelLayout)
{
	const SScoreboardTeamLabelLayout Layout = ResolveScoreboardTeamLabelLayout(
		220.0f,
		40.0f,
		25.0f,
		2.5f,
		8.0f,
		SCOREBOARD_TEAM_MODE_ICON_SIZE,
		true);

	EXPECT_FLOAT_EQ(SCOREBOARD_TEAM_MODE_ICON_SIZE, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_RowSpacing, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_Y, 67.0f);
	EXPECT_FLOAT_EQ(Layout.m_IconY, 65.0f);
}

TEST(QmScoreboardRender, DdTeamLabelSpacingFitsDenseColumnsWithoutOverlap)
{
	constexpr float AvailableRowsHeight = 333.0f;
	constexpr int RowsPerColumn = 12;
	constexpr float PreferredLineHeight = 25.0f;
	constexpr float PreferredSpacing = 2.5f;
	constexpr float PreferredTeamFontSize = 8.0f;
	const float ScaleWithoutTeams = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, 0, 0, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const float Scale = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, RowsPerColumn, RowsPerColumn, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const SScoreboardTeamLabelLayout TeamEnd = ResolveScoreboardTeamLabelLayout(
		0.0f,
		0.0f,
		PreferredLineHeight * Scale,
		PreferredSpacing * Scale,
		PreferredTeamFontSize * Scale,
		SCOREBOARD_TEAM_MODE_ICON_SIZE * Scale,
		true);

	EXPECT_FLOAT_EQ(ScaleWithoutTeams, 1.0f);
	EXPECT_LT(Scale, 1.0f);
	EXPECT_FLOAT_EQ(TeamEnd.m_RowSpacing, SCOREBOARD_TEAM_MODE_ICON_SIZE * Scale);
	EXPECT_LE(RowsPerColumn * (PreferredLineHeight * Scale + TeamEnd.m_RowSpacing), AvailableRowsHeight + 0.001f);
}

TEST(QmScoreboardRender, DenseTeamModeRowsRemainVisibleInsideTheColumn)
{
	const CUIRect Column = {20.0f, 105.0f, 280.0f, 355.0f};
	const CUIRect Rows = ScoreboardPlayerRowsRect(Column, 22.0f);
	constexpr int RowCount = 43;
	constexpr float PreferredLineHeight = 7.5f;
	constexpr float PreferredFontSize = 7.0f;
	const float Scale = ScoreboardRowsVerticalScale(Rows.h, RowCount, RowCount, RowCount,
		PreferredLineHeight, 0.0f, PreferredFontSize / 1.5f, PreferredFontSize);

	ASSERT_GT(Scale, 0.0f);
	ASSERT_LE(Scale, 1.0f);
	EXPECT_GT(Rows.y, Column.y);
	EXPECT_LT(Rows.y + Rows.h, Column.y + Column.h);
	float RowY = Rows.y;
	for(int RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		SCOPED_TRACE(RowIndex);
		const float LineHeight = PreferredLineHeight * Scale;
		const float TeamFontSize = PreferredFontSize * Scale / 1.5f;
		const float IconSize = PreferredFontSize * Scale;
		const SScoreboardTeamLabelLayout Label = ResolveScoreboardTeamLabelLayout(
			Rows.x, RowY, LineHeight, 0.0f, TeamFontSize, IconSize, true);
		RowY += LineHeight + Label.m_RowSpacing;
		EXPECT_LE(Label.m_Y + TeamFontSize, RowY + 0.001f);
		EXPECT_LE(Label.m_IconY + IconSize, RowY + 0.001f);
		EXPECT_LE(RowY, Rows.y + Rows.h + 0.001f);
	}
}

TEST(QmScoreboardRender, RowsFitThePanelDuringItsScaleAnimation)
{
	for(const float PanelScale : {0.985f, 1.0f, 1.015f})
	{
		SCOPED_TRACE(PanelScale);
		const CUIRect Column = {20.0f, 105.0f, 450.0f, 385.0f * PanelScale - 30.0f};
		const CUIRect Rows = ScoreboardPlayerRowsRect(Column, 22.0f);
		const float Scale = ScoreboardRowsVerticalScale(Rows.h, 16, 16, 0, 20.0f, 0.0f, 8.0f, 12.0f);
		const SScoreboardTeamLabelLayout LastLabel = ResolveScoreboardTeamLabelLayout(
			Rows.x, Rows.y + 15.0f * 28.0f * Scale, 20.0f * Scale, 0.0f, 8.0f * Scale, 0.0f, true);
		EXPECT_LE(LastLabel.m_Y + 8.0f * Scale, Rows.y + Rows.h + 0.001f);
		EXPECT_LT(Rows.y + Rows.h, Column.y + Column.h);
	}
}

TEST(QmScoreboardRender, DeadTeeFitsItsRowAndDoesNotCoverTheTeamLabel)
{
	const CUIRect Row = {20.0f, 410.0f, 400.0f, 20.0f};
	const CUIRect Tee = ScoreboardDeadTeeRect(Row, 100.0f, 24.0f, 25.6f);
	EXPECT_GE(Tee.x, 100.0f);
	EXPECT_LE(Tee.x + Tee.w, 124.0f);
	EXPECT_GE(Tee.y, Row.y);
	EXPECT_LE(Tee.y + Tee.h, Row.y + Row.h);
	EXPECT_FLOAT_EQ(Tee.w, Tee.h);
}

TEST(QmScoreboardRender, SmallDeadTeeKeepsItsSizeAndIsCentered)
{
	const CUIRect Row = {20.0f, 410.0f, 400.0f, 30.0f};
	const CUIRect Tee = ScoreboardDeadTeeRect(Row, 100.0f, 30.0f, 16.0f);
	EXPECT_FLOAT_EQ(Tee.w, 16.0f);
	EXPECT_FLOAT_EQ(Tee.h, 16.0f);
	EXPECT_FLOAT_EQ(Tee.Center().x, 115.0f);
	EXPECT_FLOAT_EQ(Tee.Center().y, Row.Center().y);
}

TEST(QmScoreboardLayout, EightyOnePlayersWithSparseTeamsUseAllThreeColumns)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	// 三个二人队伍夹在 Team0 玩家中，不能按 128 人容量把最后一列留空。
	for(const int Start : {10, 38, 65})
	{
		aRows[Start] = {Start, false, false};
		aRows[Start + 1] = {Start, true, true};
	}
	const auto Columns = QmScoreboardColumns(aRows, 81, 1050.0f, 388.0f, 310.0f);
	ASSERT_FALSE(Columns.m_Scroll);
	ASSERT_EQ(Columns.m_Count, 3);
	int First = 0;
	for(int i = 0; i < Columns.m_Count; ++i)
	{
		const int End = Columns.m_aEndRows[i];
		EXPECT_GT(End, First);
		EXPECT_LE(End - First, 29);
		int Labels = 0;
		for(int Row = First; Row < End; ++Row)
			Labels += aRows[Row].m_TeamLabel;
		const auto Metrics = QmScoreboardRowMetrics(388.0f, End - First, Labels, Labels, false);
		EXPECT_GE(Metrics.m_FontSize, SQmScoreboardColumns::MIN_FONT_SIZE);
		EXPECT_GT(Metrics.m_LineHeight, 7.5f);
		EXPECT_LE(ScoreboardRowsHeight(End - First, Labels, Labels, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, Metrics.m_FontSize), 388.001f);
		if(End < 81)
			EXPECT_TRUE(aRows[End - 1].m_DDTeam == TEAM_FLOCK || aRows[End - 1].m_DDTeam != aRows[End].m_DDTeam);
		First = End;
	}
	EXPECT_EQ(First, 81);
}

TEST(QmScoreboardLayout, FullServerUsesActualRangesAndScrollsWhenThreeColumnsAreUnreadable)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	const auto Tall = QmScoreboardColumns(aRows, 127, 1050.0f, 800.0f, 310.0f);
	ASSERT_FALSE(Tall.m_Scroll);
	ASSERT_EQ(Tall.m_Count, 3);
	EXPECT_EQ(Tall.m_aEndRows[2], 127);
	int First = 0;
	for(int i = 0; i < Tall.m_Count; ++i)
	{
		EXPECT_GT(Tall.m_aEndRows[i], First);
		EXPECT_LE(Tall.m_aEndRows[i] - First, 43);
		First = Tall.m_aEndRows[i];
	}
	const auto Normal = QmScoreboardColumns(aRows, 127, 1050.0f, 388.0f, 310.0f);
	EXPECT_TRUE(Normal.m_Scroll);
	EXPECT_EQ(Normal.m_Count, 1);
	EXPECT_EQ(Normal.m_aEndRows[0], 127);
	const auto Metrics = QmScoreboardRowMetrics(388.0f, 127, 0, 0, Normal.m_Scroll);
	EXPECT_GE(Metrics.m_FontSize, SQmScoreboardColumns::MIN_FONT_SIZE);
	EXPECT_GT(ScoreboardRowsHeight(127, 0, 0, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, Metrics.m_FontSize), 388.0f);
}

TEST(QmScoreboardLayout, LowViewportUsesRealSparseRowHeightForTwelvePlayers)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	const auto Columns = QmScoreboardColumns(aRows, 12, 900.0f, 200.0f, 300.0f);
	ASSERT_FALSE(Columns.m_Scroll);
	ASSERT_EQ(Columns.m_Count, 3);
	int First = 0;
	for(int i = 0; i < Columns.m_Count; ++i)
	{
		const auto Metrics = QmScoreboardRowMetrics(200.0f, Columns.m_aEndRows[i] - First, 0, 0, false);
		EXPECT_FLOAT_EQ(Metrics.m_FontSize, 12.0f);
		EXPECT_LE(ScoreboardRowsHeight(Columns.m_aEndRows[i] - First, 0, 0, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, Metrics.m_FontSize), 200.0f);
		First = Columns.m_aEndRows[i];
	}
	EXPECT_EQ(First, 12);
}

TEST(QmScoreboardLayout, OneLargeTeamStaysCompleteAndFallsBackToReadableScroll)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	for(int i = 0; i < 127; ++i)
		aRows[i] = {7, i == 126, i == 126};
	const auto Columns = QmScoreboardColumns(aRows, 127, 1050.0f, 388.0f, 310.0f);
	EXPECT_TRUE(Columns.m_Scroll);
	EXPECT_EQ(Columns.m_Count, 1);
	EXPECT_EQ(Columns.m_aEndRows[0], 127);
	const auto Metrics = QmScoreboardRowMetrics(388.0f, 127, 1, 1, Columns.m_Scroll);
	EXPECT_FLOAT_EQ(Metrics.m_FontSize, 12.0f);
	EXPECT_FLOAT_EQ(Metrics.m_LineHeight, 20.0f);
}

TEST(QmScoreboardLayout, ScreenshotLargeTeamUsesThreeColumnsBeforeScrollFallback)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	for(int i = 20; i < 73; ++i)
		aRows[i] = {1, i == 72, i == 72};
	const auto Columns = QmScoreboardColumns(aRows, 81, 1050.0f, 388.0f, 310.0f);
	ASSERT_FALSE(Columns.m_Scroll);
	ASSERT_EQ(Columns.m_Count, 3);
	int First = 0;
	for(int Column = 0; Column < Columns.m_Count; ++Column)
	{
		const int End = Columns.m_aEndRows[Column];
		EXPECT_GT(End, First);
		EXPECT_LE(End - First, 29);
		const int Labels = First <= 72 && End > 72 ? 1 : 0;
		const auto Metrics = QmScoreboardRowMetrics(388.0f, End - First, Labels, Labels, false);
		EXPECT_GE(Metrics.m_FontSize, SQmScoreboardColumns::MIN_FONT_SIZE);
		First = End;
	}
	EXPECT_EQ(First, 81);
}

TEST(QmScoreboardLayout, CrowdedRowsUseAddedPanelHeightWithoutOversizedText)
{
	for(const int Count : {17, 24, 27, 43})
	{
		SCOPED_TRACE(Count);
		for(const float Height : {388.0f, 500.0f, 800.0f})
		{
			SCOPED_TRACE(Height);
			const auto Metrics = QmScoreboardRowMetrics(Height, Count, 3, 2, false);
			const float ActualHeight = ScoreboardRowsHeight(Count, 3, 2, Metrics.m_LineHeight, Metrics.m_Spacing, Metrics.m_FontSize / 1.5f, minimum(SCOREBOARD_TEAM_MODE_ICON_SIZE, Metrics.m_FontSize));
			EXPECT_NEAR(ActualHeight, Height, 0.001f);
			EXPECT_LE(Metrics.m_FontSize, 15.0f);
		}
	}
	const auto Small = QmScoreboardRowMetrics(800.0f, 8, 0, 0, false);
	EXPECT_FLOAT_EQ(Small.m_LineHeight, 30.0f);
	EXPECT_FLOAT_EQ(Small.m_FontSize, 12.0f);
	const auto Scroll = QmScoreboardRowMetrics(800.0f, 81, 3, 2, true);
	EXPECT_FLOAT_EQ(Scroll.m_LineHeight, 20.0f);
	EXPECT_FLOAT_EQ(Scroll.m_FontSize, 12.0f);
}

TEST(QmScoreboardLayout, WidthAndTeamBoundariesLimitColumnsWithoutReordering)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	for(int i = 0; i < 60; ++i)
		aRows[i] = {1 + i / 20, i % 20 == 19, i % 20 == 19};
	const auto Wide = QmScoreboardColumns(aRows, 60, 1050.0f, 388.0f, 310.0f);
	ASSERT_FALSE(Wide.m_Scroll);
	ASSERT_EQ(Wide.m_Count, 3);
	EXPECT_EQ(Wide.m_aEndRows[0], 20);
	EXPECT_EQ(Wide.m_aEndRows[1], 40);
	EXPECT_EQ(Wide.m_aEndRows[2], 60);
	const auto Narrow = QmScoreboardColumns(aRows, 60, 600.0f, 388.0f, 310.0f);
	EXPECT_TRUE(Narrow.m_Scroll);
	EXPECT_EQ(Narrow.m_Count, 1);
	EXPECT_EQ(Narrow.m_aEndRows[0], 60);
}

TEST(QmScoreboardLayout, EmptyAndSparseBoardsKeepOneColumnAndPreferredFont)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	for(const int Count : {0, 1, 4, 8, 12, 16})
	{
		SCOPED_TRACE(Count);
		const auto Columns = QmScoreboardColumns(aRows, Count, 1050.0f, 388.0f, 310.0f);
		EXPECT_FALSE(Columns.m_Scroll);
		EXPECT_EQ(Columns.m_Count, 1);
		EXPECT_EQ(Columns.m_aEndRows[0], Count);
		const auto Metrics = QmScoreboardRowMetrics(388.0f, Count, 0, 0, false);
		EXPECT_FLOAT_EQ(Metrics.m_FontSize, 12.0f);
		EXPECT_GE(Metrics.m_LineHeight, 20.0f);
	}
}

TEST(QmScoreboardLayout, CacheReusesPlansAndInvalidatesHeightWidthLabelsTeamsAndCount)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	CQmScoreboardLayoutCache Cache;
	ASSERT_EQ(Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f).m_Count, 3);
	const auto Revision = Cache.Revision();
	for(int Frame = 0; Frame < 100; ++Frame)
		EXPECT_EQ(Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f).m_Count, 3);
	EXPECT_EQ(Cache.Revision(), Revision);
	aRows[80].m_TeamLabel = true;
	Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f);
	EXPECT_EQ(Cache.Revision(), Revision + 1);
	aRows[80].m_TeamModeLabel = true;
	Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f);
	EXPECT_EQ(Cache.Revision(), Revision + 2);
	aRows[80].m_DDTeam = 5;
	Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f);
	EXPECT_EQ(Cache.Revision(), Revision + 3);
	EXPECT_TRUE(Cache.Resolve(aRows, 81, 600.0f, 388.0f, 310.0f).m_Scroll);
	EXPECT_EQ(Cache.Revision(), Revision + 4);
	Cache.Resolve(aRows, 81, 600.0f, 800.0f, 310.0f);
	EXPECT_EQ(Cache.Revision(), Revision + 5);
	Cache.Resolve(aRows, 81, 600.0f, 800.0f, 300.0f);
	EXPECT_EQ(Cache.Revision(), Revision + 6);
	EXPECT_FALSE(Cache.Resolve(aRows, 4, 600.0f, 800.0f, 300.0f).m_Scroll);
	EXPECT_EQ(Cache.Revision(), Revision + 7);
	EXPECT_EQ(Cache.Resolve(aRows, 81, 1050.0f, 388.0f, 310.0f).m_Count, 3);
}

TEST(QmScoreboardLayout, IdentityColumnsShareAllRemainingWidthAndNeverGoNegative)
{
	const auto Small = QmScoreboardIdentityWidths(100.0f, true);
	const auto Large = QmScoreboardIdentityWidths(300.0f, true);
	EXPECT_GT(Large.m_Name, Small.m_Name);
	EXPECT_GT(Large.m_Clan, Small.m_Clan);
	EXPECT_FLOAT_EQ(Large.m_Name + Large.m_Clan, 300.0f);
	EXPECT_FLOAT_EQ(QmScoreboardIdentityWidths(300.0f, false).m_Name, 300.0f);
	EXPECT_FLOAT_EQ(QmScoreboardIdentityWidths(300.0f, false).m_Clan, 0.0f);
	const auto Unavailable = QmScoreboardIdentityWidths(-10.0f, true);
	EXPECT_FLOAT_EQ(Unavailable.m_Name, 0.0f);
	EXPECT_FLOAT_EQ(Unavailable.m_Clan, 0.0f);
}

TEST(QmScoreboardLayout, DemoCursorUnlockRoutesWheelAndSliderEventsThroughProductionInput)
{
	struct CInputObserver
	{
		int m_Count = 0;
		int m_Key = 0;
		bool OnInput(const IInput::CEvent &Event)
		{
			++m_Count;
			m_Key = Event.m_Key;
			return false;
		}
	} Ui;
	bool Unlocked = false;
	ASSERT_TRUE(QmScoreboardToggleCursor(Unlocked, true, false, IClient::STATE_DEMOPLAYBACK));
	ASSERT_TRUE(Unlocked);
	for(const int Key : {KEY_MOUSE_WHEEL_DOWN, KEY_MOUSE_WHEEL_UP, KEY_MOUSE_1})
	{
		IInput::CEvent Event{};
		Event.m_Key = Key;
		Event.m_Flags = IInput::FLAG_PRESS;
		EXPECT_TRUE(QmScoreboardUiInput(Ui, QmScoreboardCursorInputEnabled(true, Unlocked, IClient::STATE_DEMOPLAYBACK), Event));
		EXPECT_EQ(Ui.m_Key, Key);
	}
	EXPECT_EQ(Ui.m_Count, 3);
	ASSERT_TRUE(QmScoreboardToggleCursor(Unlocked, true, false, IClient::STATE_DEMOPLAYBACK));
	EXPECT_FALSE(Unlocked);
	IInput::CEvent Event{};
	Event.m_Key = KEY_MOUSE_WHEEL_DOWN;
	Event.m_Flags = IInput::FLAG_PRESS;
	EXPECT_FALSE(QmScoreboardUiInput(Ui, QmScoreboardCursorInputEnabled(true, Unlocked, IClient::STATE_DEMOPLAYBACK), Event));
	EXPECT_EQ(Ui.m_Count, 3);
}

TEST(QmScoreboardLayout, CursorToggleRejectsClosedScoreboardOpenMenusAndOfflineStates)
{
	bool Unlocked = false;
	EXPECT_FALSE(QmScoreboardToggleCursor(Unlocked, false, false, IClient::STATE_DEMOPLAYBACK));
	EXPECT_FALSE(QmScoreboardToggleCursor(Unlocked, true, true, IClient::STATE_DEMOPLAYBACK));
	EXPECT_FALSE(QmScoreboardToggleCursor(Unlocked, true, false, IClient::STATE_OFFLINE));
	EXPECT_FALSE(Unlocked);
	EXPECT_TRUE(QmScoreboardToggleCursor(Unlocked, true, false, IClient::STATE_ONLINE));
	EXPECT_TRUE(QmScoreboardCursorInputEnabled(true, Unlocked, IClient::STATE_ONLINE));
	EXPECT_FALSE(QmScoreboardCursorInputEnabled(false, Unlocked, IClient::STATE_ONLINE));
	EXPECT_FALSE(QmScoreboardCursorInputEnabled(true, Unlocked, IClient::STATE_OFFLINE));
}

TEST(QmScoreboardLayout, AutomaticScrollClickEnablesManualAndDisablingManualRestoresAuto)
{
	auto Mode = QmScoreboardScrollMode(false, true);
	ASSERT_EQ(Mode, EQmScoreboardScrollMode::AUTO);
	const int ManualEnabled = QmScoreboardManualScrollAfterClick(Mode);
	EXPECT_EQ(ManualEnabled, 1);
	Mode = QmScoreboardScrollMode(ManualEnabled != 0, true);
	ASSERT_EQ(Mode, EQmScoreboardScrollMode::MANUAL);
	const int RestoreAuto = QmScoreboardManualScrollAfterClick(Mode);
	EXPECT_EQ(RestoreAuto, 0);
	EXPECT_EQ(QmScoreboardScrollMode(RestoreAuto != 0, true), EQmScoreboardScrollMode::AUTO);
}

TEST(QmScoreboardLayout, ManualScrollClickKeepsExistingOnOffBehaviorWhenRowsFit)
{
	auto Mode = QmScoreboardScrollMode(false, false);
	ASSERT_EQ(Mode, EQmScoreboardScrollMode::FIT);
	Mode = QmScoreboardScrollMode(QmScoreboardManualScrollAfterClick(Mode) != 0, false);
	ASSERT_EQ(Mode, EQmScoreboardScrollMode::MANUAL);
	EXPECT_EQ(QmScoreboardScrollMode(QmScoreboardManualScrollAfterClick(Mode) != 0, false), EQmScoreboardScrollMode::FIT);
}

TEST(QmScoreHudLayout, ShortRankKeepsOriginalFootprint)
{
	const auto Layout = QmScoreHudLayout(300.0f, 14.0f, 7.0f, 18.0f);
	EXPECT_FLOAT_EQ(Layout.m_BoxLeft, 248.0f);
	EXPECT_FLOAT_EQ(Layout.m_BoxWidth, 52.0f);
	EXPECT_FLOAT_EQ(Layout.m_RankX, 251.0f);
	EXPECT_FLOAT_EQ(Layout.m_TeeX, 274.0f);
}

TEST(QmScoreHudLayout, MeasuredRanksLeaveSpaceBeforeTee)
{
	// 宽度由渲染器测量，包含名次末尾的句点，覆盖短名次到三位数名次。
	for(const float RankTextWidth : {7.0f, 12.0f, 14.0f, 19.0f, 21.0f, 24.0f})
	{
		for(const float ScoreWidth : {14.0f, 70.0f})
		{
			const auto Layout = QmScoreHudLayout(300.0f, ScoreWidth, RankTextWidth, 18.0f);
			const float RankRight = Layout.m_RankX + RankTextWidth;
			const float TeeLeft = Layout.m_TeeX - 9.0f;
			EXPECT_GE(TeeLeft - RankRight, 3.0f);
			EXPECT_FLOAT_EQ(Layout.m_RankX - Layout.m_BoxLeft, 3.0f);
			EXPECT_FLOAT_EQ(Layout.m_BoxLeft + Layout.m_BoxWidth, 300.0f);
		}
	}
}

TEST(QmScoreHudLayout, WiderRankExpandsOnlyToTheLeft)
{
	const auto TwoDigits = QmScoreHudLayout(300.0f, 14.0f, 14.0f, 18.0f);
	const auto ThreeDigits = QmScoreHudLayout(300.0f, 14.0f, 21.0f, 18.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_TeeX, TwoDigits.m_TeeX);
	EXPECT_FLOAT_EQ(ThreeDigits.m_BoxLeft, TwoDigits.m_BoxLeft - 7.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_BoxWidth, TwoDigits.m_BoxWidth + 7.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_RankX, TwoDigits.m_RankX - 7.0f);
}

TEST(QmInputOverlayLayout, MouseClassificationRequiresMouseOnlyInputs)
{
	EXPECT_TRUE(QmInputOverlay::IsMouseOnlyLayout(false, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, false));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(false, false));
}

TEST(QmInputOverlayLayout, MouseSizeDoesNotMoveKeyboardOrMouseAnchor)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.1f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	EXPECT_FLOAT_EQ(Keyboard.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(Keyboard.m_MaxX, 216.0f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX, LargeMouse.m_MinX);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX - Keyboard.m_MaxX, 17.5f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MaxX - SmallMouse.m_MinX, 28.5f);
	EXPECT_FLOAT_EQ(LargeMouse.m_MaxX - LargeMouse.m_MinX, 142.5f);
}

TEST(QmInputOverlayLayout, VisibleBoundsUseIndependentContentScales)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.25f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	const auto SmallBounds = QmInputOverlay::UnionBounds(Keyboard, SmallMouse);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinY, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxX, 304.75f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxY, 150.0f);

	const auto LargeBounds = QmInputOverlay::UnionBounds(Keyboard, LargeMouse);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxX, 376.0f);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxY, 210.5f);
}

TEST(QmInputOverlayFiles, PendingCheckDoesNotWaitOrPublishPartialTime)
{
	CSemaphore Started;
	CSemaphore Finish;
	CJobPool Pool;
	Pool.Init(1);
	auto pCheck = std::make_shared<CQmInputOverlayFileTimeJob>([&]() -> std::optional<time_t> {
		Started.Signal();
		Finish.Wait();
		return 123;
	});
	std::optional<time_t> Modified = 99;
	Pool.Add(pCheck);
	Started.Wait();
	EXPECT_FALSE(pCheck->TryGetResult(Modified));
	EXPECT_EQ(Modified, 99);
	Finish.Signal();
	Pool.Shutdown();
	EXPECT_TRUE(pCheck->TryGetResult(Modified));
	EXPECT_EQ(Modified, 123);
}

TEST(QmInputOverlayFiles, MissingFileIsACompletedResult)
{
	CJobPool Pool;
	Pool.Init(1);
	auto pCheck = std::make_shared<CQmInputOverlayFileTimeJob>([] { return std::optional<time_t>(); });
	Pool.Add(pCheck);
	Pool.Shutdown();
	std::optional<time_t> Modified = 99;
	EXPECT_TRUE(pCheck->TryGetResult(Modified));
	EXPECT_FALSE(Modified.has_value());
}

TEST(QmCountryFlags, SelectionCanResetToDefaultAndThenChooseAnotherCountry)
{
	int Country = 156;
	EXPECT_TRUE(QmCommitCountrySelection(&Country, CountryCode::DEFAULT));
	EXPECT_EQ(Country, CountryCode::DEFAULT);
	EXPECT_TRUE(QmCommitCountrySelection(&Country, CountryCode::DEFAULT));
	EXPECT_TRUE(QmCommitCountrySelection(&Country, 840));
	EXPECT_EQ(Country, 840);
}

TEST(QmCountryFlags, InvalidSelectionDoesNotOverwritePlayerCountry)
{
	int Country = 156;
	EXPECT_FALSE(QmCommitCountrySelection(&Country, CountryCode::MINIMUM - 1));
	EXPECT_FALSE(QmCommitCountrySelection(&Country, CountryCode::MAXIMUM + 1));
	EXPECT_FALSE(QmCommitCountrySelection(nullptr, CountryCode::DEFAULT));
	EXPECT_EQ(Country, 156);
}

TEST(QmIconButtonGeometry, WideAndTallSlotsProduceCenteredSquareHitRegions)
{
	for(const CUIRect Slot : {CUIRect{10.0f, 20.0f, 50.0f, 20.0f}, CUIRect{10.0f, 20.0f, 20.0f, 50.0f}, CUIRect{0.0f, 0.0f, 16.0f, 16.0f}})
	{
		const CUIRect Button = QmUiSquareIconButtonRect(Slot);
		EXPECT_FLOAT_EQ(Button.w, Button.h);
		EXPECT_LE(Button.w, Slot.w);
		EXPECT_LE(Button.h, Slot.h);
		EXPECT_FLOAT_EQ(Button.x + Button.w * 0.5f, Slot.x + Slot.w * 0.5f);
		EXPECT_FLOAT_EQ(Button.y + Button.h * 0.5f, Slot.y + Slot.h * 0.5f);
		EXPECT_GE(Button.x, Slot.x);
		EXPECT_GE(Button.y, Slot.y);
	}
}

TEST(QmSecondaryPanelTheme, UserColorOpacityAndBorderControlTheSurface)
{
	const SUiTheme Base = ResolveSecondaryPanelTheme(0x97FFA6, 35, 0xFFFFFF);
	const SUiTheme Other = ResolveSecondaryPanelTheme(0x000000, 80, 0xFFFFFF);
	EXPECT_FLOAT_EQ(Base.m_Surface.a, 0.35f);
	EXPECT_FLOAT_EQ(Other.m_Surface.a, 0.80f);
	EXPECT_NE(Base.m_Surface.g, Other.m_Surface.g);
	EXPECT_FLOAT_EQ(ResolveSecondaryPanelTheme(0, -1, 0).m_Surface.a, 0.0f);
	EXPECT_FLOAT_EQ(ResolveSecondaryPanelTheme(0, 101, 0).m_Surface.a, 1.0f);
	const ColorRGBA Border = color_cast<ColorRGBA>(ColorHSLA(0xFFFFFF, true));
	EXPECT_FLOAT_EQ(Base.m_Border.r, Border.r);
	EXPECT_FLOAT_EQ(Base.m_Border.a, Border.a);
}

TEST(QmDropdownSurface, UserColorAndOpacityAreIndependentFromPopupSurface)
{
	const ColorRGBA Button = ResolveDropdownSurface(0x97FFA6, 35);
	const SUiTheme Popup = ResolveSecondaryPanelTheme(0x000000, 80, 0xFFFFFF);
	EXPECT_FLOAT_EQ(Button.a, 0.35f);
	EXPECT_FLOAT_EQ(Popup.m_Surface.a, 0.80f);
	EXPECT_NE(Button.g, Popup.m_Surface.g);
	EXPECT_FLOAT_EQ(ResolveDropdownSurface(0, -1).a, 0.0f);
	EXPECT_FLOAT_EQ(ResolveDropdownSurface(0, 101).a, 1.0f);
}

TEST(QmSurfaceForeground, OpaqueLightAndDarkSurfacesHaveReadableForeground)
{
	EXPECT_EQ(ResolveUiSurfaceForeground(ColorRGBA(1, 1, 1, 1)), ColorRGBA(0, 0, 0, 1));
	EXPECT_EQ(ResolveUiSurfaceForeground(ColorRGBA(0, 0, 0, 1)), ColorRGBA(1, 1, 1, 1));
	for(int Gray = 0; Gray <= 100; ++Gray)
	{
		const float Value = Gray / 100.0f;
		const ColorRGBA Surface(Value, Value, Value, 1);
		const float Background = UiSurfaceLuminance(Surface);
		const float Foreground = UiSurfaceLuminance(ResolveUiSurfaceForeground(Surface));
		EXPECT_GE((std::max(Background, Foreground) + 0.05f) / (std::min(Background, Foreground) + 0.05f), 4.5f) << Gray;
	}
}

TEST(QmSurfaceForeground, TransparencyUsesBackdropInsteadOfInvisibleRgb)
{
	const ColorRGBA Light(1, 1, 1, 0);
	const ColorRGBA Dark(0, 0, 0, 0);
	EXPECT_EQ(ResolveUiSurfaceForeground(Light), ResolveUiSurfaceForeground(Dark));
	EXPECT_EQ(ResolveUiSurfaceForeground(Light, ColorRGBA(1, 1, 1, 1)), ColorRGBA(0, 0, 0, 1));
	EXPECT_EQ(ResolveUiSurfaceForeground(Light, Light), ResolveUiSurfaceForeground(Light, Dark));
	EXPECT_EQ(ResolveUiSurfaceForeground(Light.WithAlpha(0.2f)), ColorRGBA(1, 1, 1, 1));
	EXPECT_EQ(ResolveUiSurfaceForeground(Light.WithAlpha(0.9f)), ColorRGBA(0, 0, 0, 1));
}

TEST(QmSurfaceForeground, ThemeResolvesAfterUserOpacity)
{
	const unsigned White = ColorHSLA(0, 0, 1, 1).Pack(false);
	const SUiTheme Light = ResolveSecondaryPanelTheme(White, 100, 0);
	EXPECT_EQ(Light.m_TextBody, ResolveUiSurfaceForeground(Light.m_Surface));
	EXPECT_EQ(Light.m_TextTitle, Light.m_TextBody);
	EXPECT_EQ(Light.m_TextSmall, Light.m_TextBody);
	const SUiTheme Transparent = ResolveSecondaryPanelTheme(White, 0, 0);
	EXPECT_NE(Transparent.m_TextBody, Light.m_TextBody);
	const SUiTheme Base = ResolveUiTheme(ColorHSLA(White), 1);
	EXPECT_EQ(Base.m_TextBody, ResolveUiSurfaceForeground(Base.m_Surface));
}

TEST(QmSurfaceForeground, IconOverridePreservesReadableColorAndFallsBackWhenInvisible)
{
	const ColorRGBA White(1, 1, 1, 1);
	const ColorRGBA Black(0, 0, 0, 1);
	const ColorRGBA Blue(0, 0, 0.8f, 0.65f);
	EXPECT_EQ(ResolveUiSurfaceIconColor(White, Blue), Blue);
	EXPECT_EQ(ResolveUiSurfaceIconColor(White, White.WithAlpha(0.65f)), Black.WithAlpha(0.65f));
	EXPECT_EQ(ResolveUiSurfaceIconColor(Black, Black), White);
}

TEST(QmSurfaceForeground, ConfiguredIconModesStayReadableOnLightAndDarkSurfaces)
{
	for(int Mode = 1; Mode <= 4; ++Mode)
	{
		SCOPED_TRACE(Mode);
		const ColorRGBA Preferred = QmUiIconColor(ColorRGBA(1, 1, 1, 0.75f), Mode, ColorHSLA(0.6f, 0.8f, 0.5f).Pack(false), 0.25f);
		for(const ColorRGBA Surface : {ColorRGBA(0, 0, 0, 1), ColorRGBA(1, 1, 1, 1)})
		{
			const ColorRGBA Icon = ResolveUiSurfaceIconColor(Surface, Preferred);
			EXPECT_FLOAT_EQ(Icon.a, Preferred.a);
			const float Background = UiSurfaceLuminance(Surface);
			const float Foreground = UiSurfaceLuminance(Icon);
			EXPECT_GE((std::max(Background, Foreground) + 0.05f) / (std::min(Background, Foreground) + 0.05f), 3.0f);
		}
	}
}

TEST(QmSurfaceForeground, NestedSurfaceScopeUsesParentBackdropAndRestoresOnLeaving)
{
	CUiScopedSurfaceText Parent(nullptr, ColorRGBA(1, 1, 1, 1));
	EXPECT_EQ(ResolveUiSurfaceForeground(Parent.Surface()), ColorRGBA(0, 0, 0, 1));
	{
		CUiScopedSurfaceText TransparentButton(nullptr, ColorRGBA(1, 1, 1, 0.25f));
		EXPECT_EQ(TransparentButton.Surface(), ColorRGBA(1, 1, 1, 1));
		{
			CUiScopedSurfaceText DarkPopup(nullptr, ColorRGBA(0, 0, 0, 1));
			EXPECT_EQ(ResolveUiSurfaceForeground(DarkPopup.Surface()), ColorRGBA(1, 1, 1, 1));
		}
		EXPECT_EQ(TransparentButton.Surface(), ColorRGBA(1, 1, 1, 1));
	}
	EXPECT_EQ(Parent.Surface(), ColorRGBA(1, 1, 1, 1));
}

TEST(QmSurfaceColor, PickerHexAndRenderedSurfacePreserveBlackAndSaturatedRed)
{
	for(const ColorHSVA Color : {ColorHSVA(0, 0, 0), ColorHSVA(0, 1, 1), ColorHSVA(0, 1, 0.17f)})
	{
		const unsigned Packed = color_cast<ColorHSLA>(Color).Pack(false);
		const ColorRGBA Expected = color_cast<ColorRGBA>(ColorHSLA(Packed));
		EXPECT_EQ(ResolveUiControlSurface(Packed, 100), Expected);
		EXPECT_EQ(ResolveSecondaryPanelTheme(Packed, 100, 0).m_Surface, Expected);
		EXPECT_EQ(ResolveUiTheme(ColorHSLA(Packed), 1).m_Surface, Expected);
	}
	EXPECT_EQ(ResolveUiControlSurface(0, 100), ColorRGBA(0, 0, 0, 1));
	const ColorRGBA Red = ResolveUiControlSurface(ColorHSLA(0, 1, 0.5f).Pack(false), 100);
	EXPECT_GT(Red.r, 0.99f);
	EXPECT_LT(Red.g, 0.01f);
	EXPECT_LT(Red.b, 0.01f);
}

TEST(QmColorPickerCoordinates, TopRightIsPureHueAndBottomIsBlack)
{
	const ColorRGBA Red = color_cast<ColorRGBA>(ResolveUiColorPickerSelection(0, 1, 0, 0.6f));
	EXPECT_EQ(Red, ColorRGBA(1, 0, 0, 0.6f));
	EXPECT_EQ(color_cast<ColorRGBA>(ResolveUiColorPickerSelection(0, 0, 0, 1)), ColorRGBA(1, 1, 1, 1));
	EXPECT_EQ(color_cast<ColorRGBA>(ResolveUiColorPickerSelection(0, 1, 1, 1)), ColorRGBA(0, 0, 0, 1));
	EXPECT_EQ(color_cast<ColorRGBA>(ResolveUiColorPickerSelection(0, 2, -1, 2)), ColorRGBA(1, 0, 0, 1));
}

TEST(QmControlSurface, DisablingChangesOpacityWithoutChangingSelectedRgb)
{
	const unsigned Packed = ColorHSLA(0.3f, 0.7f, 0.25f).Pack(false);
	const ColorRGBA Enabled = ResolveUiControlSurface(Packed, 80);
	const ColorRGBA Disabled = ResolveUiControlSurface(Packed, 80, false);
	EXPECT_FLOAT_EQ(Disabled.r, Enabled.r);
	EXPECT_FLOAT_EQ(Disabled.g, Enabled.g);
	EXPECT_FLOAT_EQ(Disabled.b, Enabled.b);
	EXPECT_FLOAT_EQ(Disabled.a, Enabled.a * 0.65f);
	EXPECT_EQ(ResolveUiControlSurface(0, 100, false), ColorRGBA(0, 0, 0, 0.65f));
}

TEST(QmSurfaceForeground, PopupModeButtonUsesItsDrawnSurfaceAndRestoresParent)
{
	CUiScopedSurfaceText Popup(nullptr, ColorRGBA(0, 0, 0, 1));
	{
		CUiScopedSurfaceText SelectedMode(nullptr, ColorRGBA(0, 0, 0, 0.4f));
		EXPECT_EQ(ResolveUiSurfaceForeground(SelectedMode.Surface()), ColorRGBA(1, 1, 1, 1));
	}
	{
		CUiScopedSurfaceText OtherMode(nullptr, ColorRGBA(1, 1, 1, 0.5f));
		EXPECT_EQ(ResolveUiSurfaceForeground(OtherMode.Surface()), ColorRGBA(0, 0, 0, 1));
	}
	EXPECT_EQ(Popup.Surface(), ColorRGBA(0, 0, 0, 1));
}

TEST(QmTextColorPolicy, AutomaticFollowsBackgroundButManualChoicesRemainFixed)
{
	const unsigned Custom = ColorHSLA(0.65f, 0.8f, 0.4f).Pack(false);
	const ColorRGBA Black(0, 0, 0, 1), White(1, 1, 1, 1);
	EXPECT_EQ(ResolveUiTextColor(Black, 0, Custom), White);
	EXPECT_EQ(ResolveUiTextColor(White, 0, Custom), Black);
	for(const ColorRGBA Surface : {Black, White, ColorRGBA(1, 0, 0, 0)})
	{
		EXPECT_EQ(ResolveUiTextColor(Surface, 1, Custom), White);
		EXPECT_EQ(ResolveUiTextColor(Surface, 2, Custom), Black);
		EXPECT_EQ(ResolveUiTextColor(Surface, 3, Custom), color_cast<ColorRGBA>(ColorHSLA(Custom)).WithAlpha(1));
		EXPECT_EQ(ResolveUiTextColor(Surface, -1, Custom), ResolveUiSurfaceForeground(Surface));
	}
}

TEST(QmIconButtonSurface, ExplicitStateColorsPreserveAnimationAlpha)
{
	for(const ColorRGBA StateColor : {ColorRGBA(1, 0.32f, 0.32f, 0.95f), ColorRGBA(0.82f, 0.88f, 0.96f, 0.45f)})
	{
		for(const float AnimationAlpha : {1.0f, 0.25f, 0.0f})
		{
			const ColorRGBA Color = StateColor.WithMultipliedAlpha(AnimationAlpha);
			EXPECT_EQ(ResolveConfiguredIconButtonSurface(Color), Color);
		}
	}
}

TEST(QmIconButtonSurface, DisabledNeighborRetainsExplicitStateColor)
{
	const ColorRGBA Muted(1, 0.32f, 0.32f, 0.35f);
	EXPECT_EQ(ResolveConfiguredIconButtonSurface(Muted, false), Muted);
	EXPECT_EQ(ResolveUiIconButtonFeedback(CompositeUiSurface(Muted), false, true, true), ColorRGBA(0, 0, 0, 0));
}

TEST(QmIconButtonSurface, OrdinaryButtonsUseConfiguredSurfaceAndDisabledOpacity)
{
	struct SRestoreConfig
	{
		CConfig m_Previous = g_Config;
		~SRestoreConfig() { g_Config = m_Previous; }
	} Restore;
	g_Config.m_QmUiDropdownColor = ColorHSLA(0.3f, 0.7f, 0.25f).Pack(false);
	g_Config.m_QmUiDropdownOpacity = 80;
	const ColorRGBA Expected = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmUiDropdownColor)).WithAlpha(0.8f);
	EXPECT_EQ(ResolveConfiguredIconButtonSurface(std::nullopt), Expected);
	EXPECT_EQ(ResolveConfiguredIconButtonSurface(std::nullopt, false), Expected.WithMultipliedAlpha(0.65f));
}

TEST(QmIconButtonFeedback, HoverAndPressAreVisibleOnBothSurfacesButDisabledStaysIdle)
{
	for(const ColorRGBA Surface : {ColorRGBA(0, 0, 0, 1), ColorRGBA(1, 1, 1, 1)})
	{
		const ColorRGBA Idle = ResolveUiIconButtonFeedback(Surface, true, false, false);
		const ColorRGBA Hover = ResolveUiIconButtonFeedback(Surface, true, true, false);
		const ColorRGBA Pressed = ResolveUiIconButtonFeedback(Surface, true, true, true);
		EXPECT_FLOAT_EQ(Idle.a, 0);
		EXPECT_GT(Hover.a, 0);
		EXPECT_GT(Pressed.a, Hover.a);
		EXPECT_NE(CompositeUiSurface(Hover, Surface), Surface);
		EXPECT_EQ(ResolveUiIconButtonFeedback(Surface, false, true, true), Idle);
		EXPECT_EQ(ResolveUiIconButtonFeedback(Surface, true, false, false), Idle);
	}
}

TEST(QmSurfaceRoles, ConfiguredRolesRemainIndependentAtLowAndZeroOpacity)
{
	struct SRestoreConfig
	{
		CConfig m_Previous = g_Config;
		~SRestoreConfig() { g_Config = m_Previous; }
	} Restore;
	g_Config.m_QmUiDropdownColor = ColorHSLA(0, 1, 0.5f).Pack(false);
	g_Config.m_QmUiDropdownOpacity = 100;
	g_Config.m_QmUiInputColor = ColorHSLA(0.3f, 1, 0.5f).Pack(false);
	g_Config.m_QmUiInputOpacity = 50;
	g_Config.m_QmUiDropdownListColor = ColorHSLA(0.6f, 1, 0.5f).Pack(false);
	g_Config.m_QmUiDropdownListOpacity = 10;
	g_Config.m_QmUiPopupColor = 0;
	g_Config.m_QmUiPopupOpacity = 0;
	const ColorRGBA Button = ResolveConfiguredControlSurface();
	const ColorRGBA Input = ResolveConfiguredInputSurface();
	const ColorRGBA List = ResolveConfiguredDropdownListTheme().m_Surface;
	EXPECT_EQ(ResolveConfiguredDropdownSurface(), Button);
	EXPECT_NE(Button, Input);
	EXPECT_NE(Input, List);
	EXPECT_FLOAT_EQ(List.a, 0.1f);
	EXPECT_FLOAT_EQ(ResolveConfiguredSecondaryPanelTheme().m_Surface.a, 0);
	g_Config.m_QmUiPopupColor = ColorHSLA(0, 0, 1).Pack(false);
	g_Config.m_QmUiPopupOpacity = 100;
	EXPECT_EQ(ResolveConfiguredControlSurface(), Button);
	EXPECT_EQ(ResolveConfiguredInputSurface(), Input);
	EXPECT_EQ(ResolveConfiguredDropdownListTheme().m_Surface, List);
	g_Config.m_QmUiTextColorMode = 2;
	EXPECT_EQ(ResolveConfiguredSecondaryPanelTheme().m_TextBody, ColorRGBA(0, 0, 0, 1));
	g_Config.m_QmUiPopupColor = 0;
	EXPECT_EQ(ResolveConfiguredSecondaryPanelTheme().m_TextBody, ColorRGBA(0, 0, 0, 1));
}
