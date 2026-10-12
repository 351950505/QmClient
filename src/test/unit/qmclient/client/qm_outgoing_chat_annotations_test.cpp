#include <game/client/components/qmclient/chat_translation_notes.h>

#include <gtest/gtest.h>

TEST(QmOutgoingChatAnnotations, OnlyMatchingLocalSenderTeamAndConnectionReceiveOriginal)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(1, 8, 1, "translated", "原文", 100, 30);
	EXPECT_TRUE(Notes.Consume(0, 8, 1, "translated", 101).empty());
	EXPECT_TRUE(Notes.Consume(1, 7, 1, "translated", 101).empty());
	EXPECT_TRUE(Notes.Consume(1, 8, 0, "translated", 101).empty());
	EXPECT_TRUE(Notes.Consume(1, 8, 1, "different", 101).empty());
	EXPECT_EQ(Notes.Consume(1, 8, 1, "translated", 102), "原文");
	EXPECT_TRUE(Notes.Consume(1, 8, 1, "translated", 103).empty());
}
TEST(QmOutgoingChatAnnotations, MissingOrdinaryEchoMakesRepeatedTranslationAmbiguous)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", nullptr, 100, 30);
	Notes.Record(0, 4, 0, "hello", "你好", 101, 30);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 102).empty());
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 103).empty());
}
TEST(QmOutgoingChatAnnotations, RepeatedUnacknowledgedTranslationsDoNotMisattributeOriginals)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", "你好", 100, 30);
	Notes.Record(0, 4, 0, "hello", "您好", 101, 30);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 102).empty());
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 103).empty());
}
TEST(QmOutgoingChatAnnotations, ExpiredAnnotationCannotAttachToUnrelatedLaterEcho)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", "old original", 100, 10);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 110).empty());
	EXPECT_EQ(Notes.Size(), 0u);
	Notes.Record(0, 4, 0, "hello", "new original", 111, 10);
	EXPECT_EQ(Notes.Consume(0, 4, 0, "hello", 120), "new original");
}
TEST(QmOutgoingChatAnnotations, ResetAndClockRollbackDiscardOldAnnotations)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", "old", 100, 30);
	Notes.Clear();
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 101).empty());
	Notes.Record(0, 4, 0, "hello", "old", 200, 30);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 150).empty());
	Notes.Record(0, 4, 0, "hello", "new", 151, 30);
	EXPECT_EQ(Notes.Consume(0, 4, 0, "hello", 152), "new");
}
TEST(QmOutgoingChatAnnotations, CapacityEvictsOldestWithoutChangingNewerOriginals)
{
	COutgoingChatAnnotations Notes;
	for(size_t Index = 0; Index <= COutgoingChatAnnotations::CAPACITY; ++Index)
	{
		const std::string Text = std::to_string(Index);
		Notes.Record(0, 4, 0, Text.c_str(), "local", 100, 100);
	}
	EXPECT_EQ(Notes.Size(), COutgoingChatAnnotations::CAPACITY);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "0", 101).empty());
	EXPECT_EQ(Notes.Consume(0, 4, 0, "32", 101), "local");
}
TEST(QmOutgoingChatAnnotations, InvalidOrEmptyMessagesDoNotCreateAnnotations)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(-1, 4, 0, "hello", "local", 100, 10);
	Notes.Record(0, -1, 0, "hello", "local", 100, 10);
	Notes.Record(0, 4, 0, "   ", "local", 100, 10);
	Notes.Record(0, 4, 0, "hello", "local", 100, 0);
	EXPECT_EQ(Notes.Size(), 0u);
}
TEST(QmOutgoingChatAnnotations, ServerTrailingWhitespaceNormalizationKeepsTheAssociation)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello　", "原文", 100, 10);
	EXPECT_EQ(Notes.Consume(0, 4, 0, "hello", 101), "原文");
}

TEST(QmOutgoingChatAnnotations, AmbiguityRemainsUntilExpiryAndFreshMessagesRecover)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", "dropped original", 100, 30);
	Notes.Record(0, 4, 0, "hello", "new original", 101, 30);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 102).empty());
	Notes.Record(0, 4, 0, "hello", "another original", 103, 30);
	EXPECT_TRUE(Notes.Consume(0, 4, 0, "hello", 104).empty());
	Notes.Record(0, 4, 0, "hello", "fresh original", 134, 30);
	EXPECT_EQ(Notes.Consume(0, 4, 0, "hello", 135), "fresh original");
}

TEST(QmOutgoingChatAnnotations, DisconnectInvalidatesOnlyTheClosedConnection)
{
	COutgoingChatAnnotations Notes;
	Notes.Record(0, 4, 0, "hello", "main original", 100, 30);
	Notes.Record(1, 8, 0, "hello", "old dummy", 100, 30);
	Notes.InvalidateConnection(1);
	EXPECT_TRUE(Notes.Consume(1, 8, 0, "hello", 101).empty());
	EXPECT_EQ(Notes.Consume(0, 4, 0, "hello", 101), "main original");
	Notes.Record(1, 8, 0, "hello", "new dummy", 102, 30);
	EXPECT_EQ(Notes.Consume(1, 8, 0, "hello", 103), "new dummy");
}
