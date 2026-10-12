#include <game/client/components/qmclient/pending_chat_messages.h>

#include <gtest/gtest.h>

#include <limits>

namespace
{
	using EAdmission = CPendingChatMessages::EAdmission;

	CPendingChatMessages::SMessage Message(const char *pText, int Connection = 0, const char *pOriginal = "")
	{
		return {1, Connection, 7, pText, pOriginal};
	}
}

TEST(QmPendingChatMessages, ReadyMessageWithoutBacklogIsSentWithoutRetention)
{
	CPendingChatMessages Pending;
	EXPECT_EQ(Pending.Admit(100, 111, 10, Message("translated", 1, "原文")), EAdmission::SENT);
	EXPECT_EQ(Pending.Size(), 0u);
	EXPECT_FALSE(Pending.PopReady(100, 112, 10).has_value());
}

TEST(QmPendingChatMessages, ExistingBacklogPreventsNewMessageFromSendingAhead)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 105, 10, Message("first")), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Admit(100, 111, 10, Message("second")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Size(), 2u);
	const auto First = Pending.PopReady(100, 111, 10);
	ASSERT_TRUE(First.has_value());
	EXPECT_EQ(First->m_Text, "first");
	const auto Second = Pending.PopReady(111, 122, 10);
	ASSERT_TRUE(Second.has_value());
	EXPECT_EQ(Second->m_Text, "second");
	EXPECT_FALSE(Pending.PopReady(122, 133, 10).has_value());
}

TEST(QmPendingChatMessages, FifoDeliversEachMessageAndItsMetadataExactlyOnce)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, {0, -1, -1, "first", ""}), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, {1, 1, 8, "second", "第二条原文"}), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, {0, 0, 7, "third", "第三条原文"}), EAdmission::QUEUED);
	const auto First = Pending.PopReady(100, 111, 10);
	ASSERT_TRUE(First.has_value());
	EXPECT_EQ(First->m_Team, 0);
	EXPECT_EQ(First->m_Connection, -1);
	EXPECT_EQ(First->m_ClientId, -1);
	EXPECT_EQ(First->m_Text, "first");
	EXPECT_TRUE(First->m_Original.empty());
	EXPECT_FALSE(Pending.PopReady(111, 112, 10).has_value());
	const auto Second = Pending.PopReady(111, 122, 10);
	ASSERT_TRUE(Second.has_value());
	EXPECT_EQ(Second->m_Team, 1);
	EXPECT_EQ(Second->m_Connection, 1);
	EXPECT_EQ(Second->m_ClientId, 8);
	EXPECT_EQ(Second->m_Text, "second");
	EXPECT_EQ(Second->m_Original, "第二条原文");
	const auto Third = Pending.PopReady(122, 133, 10);
	ASSERT_TRUE(Third.has_value());
	EXPECT_EQ(Third->m_Team, 0);
	EXPECT_EQ(Third->m_Connection, 0);
	EXPECT_EQ(Third->m_ClientId, 7);
	EXPECT_EQ(Third->m_Text, "third");
	EXPECT_EQ(Third->m_Original, "第三条原文");
	EXPECT_EQ(Pending.Size(), 0u);
	EXPECT_FALSE(Pending.PopReady(133, 144, 10).has_value());
}

TEST(QmPendingChatMessages, FullQueueRejectsEvenWhenIntervalElapsedWithoutChangingContents)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("first")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("second")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, Message("third")), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Admit(100, 111, 10, Message("rejected")), EAdmission::REJECTED);
	EXPECT_EQ(Pending.Size(), 3u);
	const auto First = Pending.PopReady(100, 111, 10);
	const auto Second = Pending.PopReady(111, 122, 10);
	const auto Third = Pending.PopReady(122, 133, 10);
	ASSERT_TRUE(First.has_value());
	ASSERT_TRUE(Second.has_value());
	ASSERT_TRUE(Third.has_value());
	EXPECT_EQ(First->m_Text, "first");
	EXPECT_EQ(Second->m_Text, "second");
	EXPECT_EQ(Third->m_Text, "third");
	EXPECT_FALSE(Pending.PopReady(133, 144, 10).has_value());
}

TEST(QmPendingChatMessages, PoppingReleasesCapacityForAnotherMessage)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("first")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("second")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, Message("third")), EAdmission::QUEUED);
	ASSERT_TRUE(Pending.PopReady(100, 111, 10).has_value());
	EXPECT_EQ(Pending.Admit(111, 112, 10, Message("fourth")), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Size(), 3u);
}

TEST(QmPendingChatMessages, ExactIntervalBoundaryDoesNotSendOrPop)
{
	CPendingChatMessages Pending;
	EXPECT_EQ(Pending.Admit(100, 110, 10, Message("boundary")), EAdmission::QUEUED);
	EXPECT_FALSE(Pending.PopReady(100, 110, 10).has_value());
	EXPECT_EQ(Pending.Size(), 1u);
	const auto Ready = Pending.PopReady(100, 111, 10);
	ASSERT_TRUE(Ready.has_value());
	EXPECT_EQ(Ready->m_Text, "boundary");
}

TEST(QmPendingChatMessages, DisconnectRemovesOnlySpecifiedConnectionAndPreservesSurvivorOrder)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("main", 0, "主身原文")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("dummy", 1, "分身原文")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, Message("active", -1)), EAdmission::QUEUED);
	Pending.InvalidateConnection(1);
	Pending.InvalidateConnection(1);
	Pending.InvalidateConnection(9);
	EXPECT_EQ(Pending.Size(), 2u);
	const auto Main = Pending.PopReady(100, 111, 10);
	const auto Active = Pending.PopReady(111, 122, 10);
	ASSERT_TRUE(Main.has_value());
	ASSERT_TRUE(Active.has_value());
	EXPECT_EQ(Main->m_Text, "main");
	EXPECT_EQ(Main->m_Original, "主身原文");
	EXPECT_EQ(Active->m_Text, "active");
	EXPECT_EQ(Active->m_Connection, -1);
	EXPECT_FALSE(Pending.PopReady(122, 133, 10).has_value());
}

TEST(QmPendingChatMessages, DisconnectRemovesEveryQueuedMessageForThatConnection)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("dummy first", 1)), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("main", 0)), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, Message("dummy second", 1)), EAdmission::QUEUED);
	Pending.InvalidateConnection(1);
	ASSERT_EQ(Pending.Size(), 1u);
	const auto Survivor = Pending.PopReady(100, 111, 10);
	ASSERT_TRUE(Survivor.has_value());
	EXPECT_EQ(Survivor->m_Text, "main");
	EXPECT_FALSE(Pending.PopReady(111, 122, 10).has_value());
}

TEST(QmPendingChatMessages, ClearDiscardsOldMessagesAndResetsClockForReuse)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 105, 10, Message("old", 1, "旧原文")), EAdmission::QUEUED);
	Pending.Clear();
	Pending.Clear();
	EXPECT_EQ(Pending.Size(), 0u);
	EXPECT_FALSE(Pending.PopReady(0, 1, 10).has_value());
	ASSERT_EQ(Pending.Admit(0, 2, 10, Message("new")), EAdmission::QUEUED);
	const auto New = Pending.PopReady(0, 11, 10);
	ASSERT_TRUE(New.has_value());
	EXPECT_EQ(New->m_Text, "new");
	EXPECT_TRUE(New->m_Original.empty());
	EXPECT_FALSE(Pending.PopReady(11, 22, 10).has_value());
}

TEST(QmPendingChatMessages, PopOnClockRollbackDiscardsBacklogWithoutSending)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(0, 100, 100, Message("old")), EAdmission::QUEUED);
	EXPECT_FALSE(Pending.PopReady(0, 99, 10).has_value());
	EXPECT_EQ(Pending.Size(), 0u);
	EXPECT_FALSE(Pending.PopReady(0, 111, 10).has_value());
}

TEST(QmPendingChatMessages, AdmissionOnClockRollbackRejectsNewMessageAndDiscardsOld)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(0, 100, 100, Message("old")), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Admit(0, 99, 10, Message("rejected")), EAdmission::REJECTED);
	EXPECT_EQ(Pending.Size(), 0u);
	EXPECT_FALSE(Pending.PopReady(0, 111, 10).has_value());
	EXPECT_EQ(Pending.Admit(111, 112, 10, Message("recovered")), EAdmission::QUEUED);
	const auto Recovered = Pending.PopReady(111, 122, 10);
	ASSERT_TRUE(Recovered.has_value());
	EXPECT_EQ(Recovered->m_Text, "recovered");
}

TEST(QmPendingChatMessages, InvalidClockInputsNeverAdmitOrReleaseMessages)
{
	struct SClock
	{
		int64_t m_LastSend;
		int64_t m_Now;
		int64_t m_Frequency;
	};
	const SClock aClocks[] = {{-1, 111, 10}, {0, -1, 10}, {112, 111, 10}, {100, 111, 0}, {100, 111, -1}};
	for(const auto &Clock : aClocks)
	{
		SCOPED_TRACE(::testing::Message() << Clock.m_LastSend << "," << Clock.m_Now << "," << Clock.m_Frequency);
		CPendingChatMessages Admission;
		ASSERT_EQ(Admission.Admit(100, 105, 10, Message("old")), EAdmission::QUEUED);
		EXPECT_EQ(Admission.Admit(Clock.m_LastSend, Clock.m_Now, Clock.m_Frequency, Message("invalid")), EAdmission::REJECTED);
		EXPECT_EQ(Admission.Size(), 0u);
		CPendingChatMessages Release;
		ASSERT_EQ(Release.Admit(100, 105, 10, Message("old")), EAdmission::QUEUED);
		EXPECT_FALSE(Release.PopReady(Clock.m_LastSend, Clock.m_Now, Clock.m_Frequency).has_value());
		EXPECT_EQ(Release.Size(), 0u);
	}
}

TEST(QmPendingChatMessages, LargeTimestampsDoNotOverflowTheIntervalCheck)
{
	constexpr int64_t Max = std::numeric_limits<int64_t>::max();
	CPendingChatMessages Ready;
	EXPECT_EQ(Ready.Admit(Max - 20, Max, 10, Message("ready")), EAdmission::SENT);
	CPendingChatMessages Waiting;
	EXPECT_EQ(Waiting.Admit(Max - 5, Max, 10, Message("waiting")), EAdmission::QUEUED);
	EXPECT_FALSE(Waiting.PopReady(Max - 5, Max, 10).has_value());
	EXPECT_EQ(Waiting.Size(), 1u);
}

TEST(QmPendingChatMessages, ReserveUsesTheSameBacklogAndCapacityAdmissionPolicy)
{
	int PendingCount = 0;
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 111, 10, PendingCount), EAdmission::SENT);
	EXPECT_EQ(PendingCount, 0);
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 110, 10, PendingCount), EAdmission::QUEUED);
	EXPECT_EQ(PendingCount, 1);
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 111, 10, PendingCount), EAdmission::QUEUED);
	EXPECT_EQ(PendingCount, 2);
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 112, 10, PendingCount), EAdmission::QUEUED);
	EXPECT_EQ(PendingCount, 3);
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 113, 10, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(PendingCount, 3);
}

TEST(QmPendingChatMessages, ReserveRejectsInvalidStateWithoutMutatingCount)
{
	int PendingCount = 2;
	EXPECT_EQ(CPendingChatMessages::Reserve(112, 111, 10, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 111, 0, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(CPendingChatMessages::Reserve(-1, 111, 10, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(PendingCount, 2);
	PendingCount = -1;
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 111, 10, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(PendingCount, -1);
	PendingCount = 4;
	EXPECT_EQ(CPendingChatMessages::Reserve(100, 111, 10, PendingCount), EAdmission::REJECTED);
	EXPECT_EQ(PendingCount, 4);
}

TEST(QmPendingChatMessages, QueueOwnsMessageDataAfterCallerChangesItsCopy)
{
	CPendingChatMessages Pending;
	auto Input = Message("translated", 1, "原文");
	ASSERT_EQ(Pending.Admit(100, 105, 10, Input), EAdmission::QUEUED);
	Input.m_Team = 0;
	Input.m_Connection = 0;
	Input.m_ClientId = 99;
	Input.m_Text = "replacement";
	Input.m_Original.clear();
	const auto Queued = Pending.PopReady(100, 111, 10);
	ASSERT_TRUE(Queued.has_value());
	EXPECT_EQ(Queued->m_Team, 1);
	EXPECT_EQ(Queued->m_Connection, 1);
	EXPECT_EQ(Queued->m_ClientId, 7);
	EXPECT_EQ(Queued->m_Text, "translated");
	EXPECT_EQ(Queued->m_Original, "原文");
	EXPECT_FALSE(Pending.PopReady(111, 122, 10).has_value());
}

TEST(QmPendingChatMessages, IdenticalBodiesRemainSeparateMessagesWithTheirOwnOriginals)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("hello", 0, "你好")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("hello", 0, "您好")), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Size(), 2u);
	const auto First = Pending.PopReady(100, 111, 10);
	const auto Second = Pending.PopReady(111, 122, 10);
	ASSERT_TRUE(First.has_value());
	ASSERT_TRUE(Second.has_value());
	EXPECT_EQ(First->m_Text, "hello");
	EXPECT_EQ(First->m_Original, "你好");
	EXPECT_EQ(Second->m_Text, "hello");
	EXPECT_EQ(Second->m_Original, "您好");
	EXPECT_FALSE(Pending.PopReady(122, 133, 10).has_value());
}

TEST(QmPendingChatMessages, DisconnectReleasesCapacityWithoutLettingNewMessageOvertakeSurvivor)
{
	CPendingChatMessages Pending;
	ASSERT_EQ(Pending.Admit(100, 101, 10, Message("dummy first", 1)), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 102, 10, Message("main", 0)), EAdmission::QUEUED);
	ASSERT_EQ(Pending.Admit(100, 103, 10, Message("dummy second", 1)), EAdmission::QUEUED);
	Pending.InvalidateConnection(1);
	ASSERT_EQ(Pending.Size(), 1u);
	EXPECT_EQ(Pending.Admit(100, 111, 10, Message("new first", 1)), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Admit(100, 112, 10, Message("new second", 0)), EAdmission::QUEUED);
	EXPECT_EQ(Pending.Admit(100, 113, 10, Message("rejected", 0)), EAdmission::REJECTED);
	const auto Survivor = Pending.PopReady(100, 113, 10);
	const auto NewFirst = Pending.PopReady(113, 124, 10);
	const auto NewSecond = Pending.PopReady(124, 135, 10);
	ASSERT_TRUE(Survivor.has_value());
	ASSERT_TRUE(NewFirst.has_value());
	ASSERT_TRUE(NewSecond.has_value());
	EXPECT_EQ(Survivor->m_Text, "main");
	EXPECT_EQ(NewFirst->m_Text, "new first");
	EXPECT_EQ(NewSecond->m_Text, "new second");
	EXPECT_FALSE(Pending.PopReady(135, 146, 10).has_value());
}

TEST(QmPendingChatMessages, ReserveHandlesMaximumClockAndFrequencyWithoutOverflow)
{
	constexpr int64_t Max = std::numeric_limits<int64_t>::max();
	int PendingCount = 0;
	EXPECT_EQ(CPendingChatMessages::Reserve(Max - 5, Max, 10, PendingCount), EAdmission::QUEUED);
	EXPECT_EQ(PendingCount, 1);
	PendingCount = 0;
	EXPECT_EQ(CPendingChatMessages::Reserve(0, Max, Max, PendingCount), EAdmission::QUEUED);
	EXPECT_EQ(PendingCount, 1);
	PendingCount = 0;
	EXPECT_EQ(CPendingChatMessages::Reserve(0, Max, Max - 1, PendingCount), EAdmission::SENT);
	EXPECT_EQ(PendingCount, 0);
}
