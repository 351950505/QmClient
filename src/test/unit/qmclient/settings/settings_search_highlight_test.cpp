#include <game/client/QmUi/SettingsSearchHighlight.h>

#include <gtest/gtest.h>

#include <limits>

TEST(SettingsSearchHighlight, MatchesTheCompleteLocalizedSettingLabel)
{
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"控制台字号"}, {"Console opacity"}});
	EXPECT_EQ(Highlight.MatchLabel("控制台字号"), 0);
	EXPECT_EQ(Highlight.MatchLabel("Console opacity"), 1);
	EXPECT_EQ(Highlight.MatchLabel("控制台"), -1);
	EXPECT_EQ(Highlight.MatchLabel(""), -1);
}

TEST(SettingsSearchHighlight, AcceptsNumericValuesWithoutMatchingOtherSettingNames)
{
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"Scale"}, {"透明度"}});
	EXPECT_EQ(Highlight.MatchLabel("Scale: 100%"), 0);
	EXPECT_EQ(Highlight.MatchLabel("Scale -1.5"), 0);
	EXPECT_EQ(Highlight.MatchLabel("透明度：24%"), 1);
	EXPECT_EQ(Highlight.MatchLabel("Scale background"), -1);
	EXPECT_EQ(Highlight.MatchLabel("Scale: custom"), -1);
	EXPECT_EQ(Highlight.MatchLabel("Scale2"), -1);
}

TEST(SettingsSearchHighlight, MatchesBothFullAndDisplayedLabelsWithHints)
{
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{" UI scale (global) "}, {"字号（控制台）"}});
	EXPECT_EQ(Highlight.MatchLabel("UI scale (global)"), 0);
	EXPECT_EQ(Highlight.MatchLabel("UI scale"), 0);
	EXPECT_EQ(Highlight.MatchLabel("UI scale: 100%"), 0);
	EXPECT_EQ(Highlight.MatchLabel("字号"), 1);
	EXPECT_EQ(Highlight.MatchLabel("字号（控制台）"), 1);
	EXPECT_EQ(Highlight.MatchLabel("UI scale (another setting)"), -1);
}

TEST(SettingsSearchHighlight, CollectsOffscreenLayoutRectsAndTheirCanonicalTarget)
{
	int ConsoleSize = 10;
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"Console font size", &ConsoleSize}});
	const CUIRect Offscreen{10.0f, 1800.0f, 130.0f, 20.0f};
	ASSERT_TRUE(Highlight.ObserveLabel("Console font size", Offscreen));
	ASSERT_EQ(Highlight.Matches().size(), 1u);
	EXPECT_EQ(Highlight.Targets()[Highlight.Matches()[0].m_TargetIndex].m_pConfig, &ConsoleSize);
	EXPECT_FLOAT_EQ(Highlight.Matches()[0].m_Rect.y, 1800.0f);
	EXPECT_FLOAT_EQ(Highlight.Matches()[0].m_Rect.h, 20.0f);
}

TEST(SettingsSearchHighlight, RepeatedPassesDoNotDuplicateTheSameMatchRect)
{
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"Scale"}});
	const CUIRect Rect{10.0f, 20.0f, 100.0f, 18.0f};
	EXPECT_TRUE(Highlight.ObserveLabel("Scale", Rect));
	EXPECT_TRUE(Highlight.ObserveLabel("Scale: 100%", Rect));
	EXPECT_EQ(Highlight.Matches().size(), 1u);
	EXPECT_TRUE(Highlight.ObserveLabel("Scale", {10.0f, 60.0f, 100.0f, 18.0f}));
	EXPECT_EQ(Highlight.Matches().size(), 2u);
}

TEST(SettingsSearchHighlight, ConfigIdentityCollectsTheActualRowWithoutGuessingALabel)
{
	int Scale = 100;
	int Opacity = 24;
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"UI scale", &Scale}, {"Opacity", &Opacity}});
	EXPECT_EQ(Highlight.MatchConfig(&Scale), 0);
	EXPECT_EQ(Highlight.MatchConfig(&Opacity), 1);
	EXPECT_EQ(Highlight.MatchConfig(nullptr), -1);
	const CUIRect Row{10.0f, 1800.0f, 260.0f, 20.0f};
	ASSERT_TRUE(Highlight.ObserveConfig(&Opacity, Row));
	ASSERT_EQ(Highlight.Matches().size(), 1u);
	EXPECT_EQ(Highlight.Matches()[0].m_TargetIndex, 1u);
	EXPECT_FLOAT_EQ(Highlight.Matches()[0].m_Rect.w, 260.0f);
}

TEST(SettingsSearchHighlight, RowAndLabelHighlightsAreDrawnOnceUntilTheNextFrame)
{
	CSettingsSearchHighlight Highlight;
	const CUIRect Row{10.0f, 20.0f, 260.0f, 20.0f};
	const CUIRect Label{10.0f, 20.0f, 130.0f, 20.0f};
	EXPECT_TRUE(Highlight.TryMarkHighlightDrawn(Row));
	EXPECT_FALSE(Highlight.TryMarkHighlightDrawn(Label));
	EXPECT_TRUE(Highlight.TryMarkHighlightDrawn({10.0f, 60.0f, 260.0f, 20.0f}));
	Highlight.ClearMatches();
	EXPECT_TRUE(Highlight.TryMarkHighlightDrawn(Label));
	EXPECT_FALSE(Highlight.TryMarkHighlightDrawn(Row));
}

TEST(SettingsSearchHighlight, RejectsUnmatchedEmptyAndNonfiniteLayoutRects)
{
	CSettingsSearchHighlight Highlight;
	Highlight.SetTargets({{"Scale"}, {""}});
	EXPECT_FALSE(Highlight.ObserveLabel("Other", {0.0f, 0.0f, 100.0f, 18.0f}));
	EXPECT_FALSE(Highlight.ObserveLabel("Scale", {0.0f, 0.0f, 0.0f, 18.0f}));
	EXPECT_FALSE(Highlight.ObserveLabel("Scale", {0.0f, std::numeric_limits<float>::quiet_NaN(), 100.0f, 18.0f}));
	EXPECT_FALSE(Highlight.ObserveLabel("", {0.0f, 0.0f, 100.0f, 18.0f}));
	EXPECT_TRUE(Highlight.Matches().empty());
}

TEST(SettingsSearchHighlight, ReplacingTargetsRemovesPreviousMatchesAndKeepsOwnedLabels)
{
	CSettingsSearchHighlight Highlight;
	std::string Label = "Scale";
	Highlight.SetTargets({{Label}});
	Label = "Unrelated";
	ASSERT_TRUE(Highlight.ObserveLabel("Scale", {0.0f, 0.0f, 100.0f, 18.0f}));
	Highlight.ClearMatches();
	EXPECT_TRUE(Highlight.Matches().empty());
	EXPECT_EQ(Highlight.MatchLabel("Scale"), 0);
	Highlight.SetTargets({{"Opacity"}});
	EXPECT_TRUE(Highlight.Matches().empty());
	EXPECT_EQ(Highlight.MatchLabel("Scale"), -1);
	EXPECT_EQ(Highlight.MatchLabel("Opacity"), 0);
}
