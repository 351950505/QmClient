#include <engine/client/friends.h>
#include <engine/shared/config.h>

#include <gtest/gtest.h>

#include <memory>

namespace
{
	class CFriendsPin : public testing::Test
	{
	protected:
		int m_PreviousIgnoreClan = g_Config.m_ClFriendsIgnoreClan;
		std::unique_ptr<CFriends> m_pFriends = std::make_unique<CFriends>();

		void SetUp() override { g_Config.m_ClFriendsIgnoreClan = 0; }
		void TearDown() override { g_Config.m_ClFriendsIgnoreClan = m_PreviousIgnoreClan; }
	};
}

TEST_F(CFriendsPin, AddedPlayerStartsUnpinned)
{
	m_pFriends->AddFriend("Alice", "Clan");
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_FALSE(m_pFriends->GetFriend(0)->m_Pinned);
}

TEST_F(CFriendsPin, ToggleChangesRevisionOnlyWhenPinStateChanges)
{
	m_pFriends->AddFriend("Alice", "Clan");
	const uint64_t Added = m_pFriends->Revision();
	EXPECT_FALSE(m_pFriends->SetFriendPinned("Alice", "Clan", false));
	EXPECT_EQ(m_pFriends->Revision(), Added);

	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_TRUE(m_pFriends->GetFriend(0)->m_Pinned);
	EXPECT_EQ(m_pFriends->Revision(), Added + 1);
	EXPECT_FALSE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	EXPECT_EQ(m_pFriends->Revision(), Added + 1);

	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", false));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_EQ(m_pFriends->Revision(), Added + 2);
}

TEST_F(CFriendsPin, UnknownPlayersAndClanMembersCannotBePinned)
{
	m_pFriends->AddFriend("", "Clan");
	const uint64_t Added = m_pFriends->Revision();
	EXPECT_FALSE(m_pFriends->SetFriendPinned("Unknown", "Other", true));
	EXPECT_FALSE(m_pFriends->SetFriendPinned("Member", "Clan", true));
	EXPECT_FALSE(m_pFriends->SetFriendPinned("", "Clan", true));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Member", "Clan"));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("", "Clan"));
	EXPECT_FALSE(m_pFriends->GetFriend(0)->m_Pinned);
	EXPECT_EQ(m_pFriends->Revision(), Added);
}

TEST_F(CFriendsPin, PlayerLookupRespectsIgnoreClanSetting)
{
	m_pFriends->AddFriend("Alice", "Original");
	EXPECT_FALSE(m_pFriends->SetFriendPinned("Alice", "Changed", true));
	g_Config.m_ClFriendsIgnoreClan = 1;
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Changed", true));
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Another"));
	g_Config.m_ClFriendsIgnoreClan = 0;
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Original"));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Changed"));
}

TEST_F(CFriendsPin, PinningOnePlayerDoesNotChangeOtherPlayers)
{
	m_pFriends->AddFriend("Alice", "First");
	m_pFriends->AddFriend("Alice", "Second");
	m_pFriends->AddFriend("Bob", "First");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "First", true));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Second"));
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Bob", "First"));
}

TEST_F(CFriendsPin, ChangingCategoryKeepsPlayerPinned)
{
	ASSERT_TRUE(m_pFriends->AddCategory("Teammates"));
	m_pFriends->AddFriend("Alice", "Clan");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	ASSERT_TRUE(m_pFriends->SetFriendCategory("Alice", "Clan", "Teammates"));
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_STREQ(m_pFriends->GetFriendCategory("Alice", "Clan"), "Teammates");
}

TEST_F(CFriendsPin, RenamingCategoryKeepsPlayerPinned)
{
	ASSERT_TRUE(m_pFriends->AddCategory("Teammates"));
	m_pFriends->AddFriend("Alice", "Clan", "Teammates");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	ASSERT_TRUE(m_pFriends->RenameCategory("Teammates", "Partners"));
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_STREQ(m_pFriends->GetFriendCategory("Alice", "Clan"), "Partners");
}

TEST_F(CFriendsPin, RemovingCategoryKeepsPinAfterFallbackToDefault)
{
	ASSERT_TRUE(m_pFriends->AddCategory("Teammates"));
	m_pFriends->AddFriend("Alice", "Clan", "Teammates");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	ASSERT_TRUE(m_pFriends->RemoveCategory("Teammates"));
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	EXPECT_STREQ(m_pFriends->GetFriendCategory("Alice", "Clan"), IFriends::DEFAULT_CATEGORY);
}

TEST_F(CFriendsPin, AddingExistingFriendKeepsPin)
{
	m_pFriends->AddFriend("Alice", "Clan");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	m_pFriends->AddFriend("Alice", "Clan");
	EXPECT_EQ(m_pFriends->NumFriends(), 1);
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Alice", "Clan"));
}

TEST_F(CFriendsPin, RemovingAndReaddingFriendDoesNotReusePin)
{
	m_pFriends->AddFriend("Alice", "Clan");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Alice", "Clan", true));
	m_pFriends->RemoveFriend("Alice", "Clan");
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Clan"));
	m_pFriends->AddFriend("Alice", "Clan");
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Alice", "Clan"));
}

TEST_F(CFriendsPin, RemovingEarlierFriendKeepsRemainingPlayersPin)
{
	m_pFriends->AddFriend("Alice", "Clan");
	m_pFriends->AddFriend("Bob", "Clan");
	ASSERT_TRUE(m_pFriends->SetFriendPinned("Bob", "Clan", true));
	m_pFriends->RemoveFriend("Alice", "Clan");
	EXPECT_TRUE(m_pFriends->IsFriendPinned("Bob", "Clan"));
	m_pFriends->AddFriend("Carol", "Clan");
	EXPECT_FALSE(m_pFriends->IsFriendPinned("Carol", "Clan"));
}
