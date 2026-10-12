#include <game/client/QmUi/SettingsSearchSession.h>

#include <gtest/gtest.h>

TEST(SettingsSearchSession, KeepsSelectedCardWhenResultsAreReorderedForSameQuery)
{
	CSettingsSearchSession Session;
	Session.SetResults("chat", {"chat", "bubble", "console"});
	ASSERT_TRUE(Session.MoveSelection(1));
	Session.SetResults("chat", {"console", "chat", "bubble"});
	EXPECT_EQ(Session.Selected(), "bubble");
	EXPECT_EQ(Session.PendingReveal(), "bubble");
}

TEST(SettingsSearchSession, NewQuerySelectsFirstResultAndCancelsOldReveal)
{
	CSettingsSearchSession Session;
	Session.SetResults("chat", {"chat", "bubble"});
	Session.MoveSelection(1);
	Session.SetResults("mouse", {"controls"});
	EXPECT_EQ(Session.Selected(), "controls");
	EXPECT_TRUE(Session.PendingReveal().empty());
}

TEST(SettingsSearchSession, RemovedSelectionFallsBackToFirstRemainingCard)
{
	CSettingsSearchSession Session;
	Session.SetResults("hud", {"coords", "island"});
	Session.MoveSelection(1);
	Session.SetResults("hud", {"coords"});
	EXPECT_EQ(Session.Selected(), "coords");
	Session.SetResults("hud", {});
	EXPECT_TRUE(Session.Selected().empty());
	EXPECT_FALSE(Session.RevealSelection());
}

TEST(SettingsSearchSession, NavigationStopsAtResultBoundaries)
{
	CSettingsSearchSession Session;
	Session.SetResults("chat", {"chat", "bubble"});
	Session.MoveSelection(-1);
	EXPECT_EQ(Session.Selected(), "chat");
	Session.MoveSelection(1);
	Session.MoveSelection(1);
	EXPECT_EQ(Session.Selected(), "bubble");
	EXPECT_EQ(Session.PendingReveal(), "bubble");
	Session.FinishReveal();
	EXPECT_TRUE(Session.PendingReveal().empty());
}

TEST(SettingsSearchSession, CompositionAndModalInputKeepSelectionAndRevealUnchanged)
{
	CSettingsSearchSession Session;
	Session.SetResults("chat", {"chat", "bubble"});
	EXPECT_FALSE(Session.MoveSelection(1, true));
	EXPECT_FALSE(Session.RevealSelection(true));
	EXPECT_FALSE(Session.MoveSelection(1, false, true));
	EXPECT_FALSE(Session.RevealSelection(false, true));
	EXPECT_EQ(Session.Selected(), "chat");
	EXPECT_TRUE(Session.PendingReveal().empty());
	EXPECT_TRUE(Session.RevealSelection());
	EXPECT_EQ(Session.PendingReveal(), "chat");
}
