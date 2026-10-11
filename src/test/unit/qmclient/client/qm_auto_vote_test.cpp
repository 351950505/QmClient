#include <game/client/components/qmclient/auto_vote.h>

#include <gtest/gtest.h>

TEST(QmAutoVote, UnknownCallerFollowsConfiguredChoice)
{
	EXPECT_EQ(QmAutoVoteChoice(1, -1, 2, false, 4, 0), -1);
	EXPECT_EQ(QmAutoVoteChoice(2, -1, 2, false, 4, 0), 1);
	EXPECT_EQ(QmAutoVoteChoice(0, -1, 2, false, 4, 0), 0);
}

TEST(QmAutoVote, KnownSelfAndFriendAreExempt)
{
	EXPECT_EQ(QmAutoVoteChoice(1, 2, 2, false, 0, 0), 0);
	EXPECT_EQ(QmAutoVoteChoice(2, 3, 2, true, 0, 0), 0);
}

TEST(QmAutoVote, OnlyTheSameNonzeroRaceTeamIsExempt)
{
	EXPECT_EQ(QmAutoVoteChoice(1, 3, 2, false, 4, 4), 0);
	EXPECT_EQ(QmAutoVoteChoice(1, 3, 2, false, 0, 0), -1);
	EXPECT_EQ(QmAutoVoteChoice(2, 3, 2, false, 4, 5), 1);
}

TEST(QmAutoVote, InvalidCallerDoesNotApplyExemptions)
{
	EXPECT_EQ(QmAutoVoteChoice(1, MAX_CLIENTS, 2, true, 4, 4), -1);
	EXPECT_EQ(QmAutoVoteChoice(2, -1, 2, true, 4, 4), 1);
}
