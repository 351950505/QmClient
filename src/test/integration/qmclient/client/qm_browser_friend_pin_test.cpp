#include <engine/client/friends.h>
#include <engine/client/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/components/qmclient/browser_friend_list.h>

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>

namespace
{
	// 只提供内存中的服务器快照，不初始化网络或请求外部服务。
	class CInMemoryFriendBrowser : public CServerBrowser
	{
		CServerInfo m_Info{};
		uint64_t m_Revision = 0;

	public:
		void SetClients(const IFriends &Friends, std::initializer_list<CServerInfo::CClient> Clients)
		{
			m_Info.m_vClients = Clients;
			m_Info.m_FriendState = IFriends::FRIEND_NO;
			for(auto &Client : m_Info.m_vClients)
			{
				Client.m_FriendState = Friends.GetFriendState(Client.m_aName, Client.m_aClan);
				m_Info.m_FriendState = maximum(m_Info.m_FriendState, Client.m_FriendState);
			}
			++m_Revision;
		}

		int NumServers() const override { return 1; }
		const CServerInfo *Get(int Index) const override { return Index == 0 ? &m_Info : nullptr; }
		uint64_t FriendListRevision() const override { return m_Revision; }
	};

	class CQmBrowserFriendPinTest : public ::testing::Test
	{
	protected:
		std::unique_ptr<CConfig> m_pSavedConfig;
		CFriends m_Friends;
		CInMemoryFriendBrowser m_Browser;
		CQmBrowserFriendList m_List;

		void SetUp() override
		{
			m_pSavedConfig = std::make_unique<CConfig>(g_Config);
			g_Config.m_ClFriendsIgnoreClan = 0;
		}

		void TearDown() override { g_Config = *m_pSavedConfig; }

		void Update() { m_List.Update(m_Friends, m_Browser, g_Config.m_ClFriendsIgnoreClan != 0); }

		static CServerInfo::CClient Client(const char *pName, const char *pClan = "Clan")
		{
			CServerInfo::CClient Result{};
			str_copy(Result.m_aName, pName);
			str_copy(Result.m_aClan, pClan);
			return Result;
		}
	};
}

TEST_F(CQmBrowserFriendPinTest, OfflinePinsPrecedeOrdinaryFriendsAndKeepNameOrder)
{
	m_Friends.AddFriend("Alice", "Clan");
	m_Friends.AddFriend("Zoe", "Clan");
	m_Friends.AddFriend("Zed", "Clan");
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "Clan", true));
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zed", "Clan", true));
	Update();

	const auto &Items = m_List.Groups()[m_Friends.FindCategory(IFriends::OFFLINE_CATEGORY)];
	ASSERT_EQ(Items.size(), 3u);
	EXPECT_STREQ(Items[0].Name(), "Zed");
	EXPECT_STREQ(Items[1].Name(), "Zoe");
	EXPECT_STREQ(Items[2].Name(), "Alice");
	EXPECT_TRUE(Items[0].IsPinned());
	EXPECT_EQ(Items[0].ServerInfo(), nullptr);
}

TEST_F(CQmBrowserFriendPinTest, OnlinePinsStayInTheirAssignedCategory)
{
	ASSERT_TRUE(m_Friends.AddCategory("Team"));
	m_Friends.AddFriend("Alice", "Clan", "Team");
	m_Friends.AddFriend("Zoe", "Clan", "Team");
	m_Friends.AddFriend("Bob", "Clan");
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "Clan", true));
	ASSERT_TRUE(m_Friends.SetFriendPinned("Bob", "Clan", true));
	m_Browser.SetClients(m_Friends, {Client("Alice"), Client("Bob"), Client("Zoe")});
	Update();

	const auto &Team = m_List.Groups()[m_Friends.FindCategory("Team")];
	ASSERT_EQ(Team.size(), 2u);
	EXPECT_STREQ(Team[0].Name(), "Zoe");
	EXPECT_STREQ(Team[1].Name(), "Alice");
	EXPECT_TRUE(Team[0].IsPinned());
	EXPECT_NE(Team[0].ServerInfo(), nullptr);
	const auto &Default = m_List.Groups()[m_Friends.FindCategory(IFriends::DEFAULT_CATEGORY)];
	ASSERT_EQ(Default.size(), 1u);
	EXPECT_STREQ(Default[0].Name(), "Bob");
	EXPECT_TRUE(m_List.Groups()[m_Friends.FindCategory(IFriends::OFFLINE_CATEGORY)].empty());
}

TEST_F(CQmBrowserFriendPinTest, ToggleRefreshesCachedOrderAndUnpinRestoresNameOrder)
{
	m_Friends.AddFriend("Alice", "Clan");
	m_Friends.AddFriend("Zoe", "Clan");
	const int Offline = m_Friends.FindCategory(IFriends::OFFLINE_CATEGORY);
	Update();
	ASSERT_EQ(m_List.Groups()[Offline].size(), 2u);
	EXPECT_STREQ(m_List.Groups()[Offline][0].Name(), "Alice");

	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "Clan", true));
	Update();
	EXPECT_STREQ(m_List.Groups()[Offline][0].Name(), "Zoe");
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "Clan", false));
	Update();
	EXPECT_STREQ(m_List.Groups()[Offline][0].Name(), "Alice");
	EXPECT_FALSE(m_List.Groups()[Offline][1].IsPinned());
}

TEST_F(CQmBrowserFriendPinTest, OnlineOfflineTransitionsKeepPinAndOriginalCategory)
{
	ASSERT_TRUE(m_Friends.AddCategory("Team"));
	m_Friends.AddFriend("Zoe", "Clan", "Team");
	m_Friends.AddFriend("Alice", "Clan");
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "Clan", true));
	const int Team = m_Friends.FindCategory("Team");
	const int Offline = m_Friends.FindCategory(IFriends::OFFLINE_CATEGORY);
	m_Browser.SetClients(m_Friends, {Client("Zoe")});
	Update();
	ASSERT_EQ(m_List.Groups()[Team].size(), 1u);
	EXPECT_TRUE(m_List.Groups()[Team][0].IsPinned());

	m_Browser.SetClients(m_Friends, {});
	Update();
	EXPECT_TRUE(m_List.Groups()[Team].empty());
	ASSERT_EQ(m_List.Groups()[Offline].size(), 2u);
	EXPECT_STREQ(m_List.Groups()[Offline][0].Name(), "Zoe");
	EXPECT_TRUE(m_List.Groups()[Offline][0].IsPinned());
	EXPECT_STREQ(m_Friends.GetFriendCategory("Zoe", "Clan"), "Team");

	m_Browser.SetClients(m_Friends, {Client("Zoe")});
	Update();
	ASSERT_EQ(m_List.Groups()[Team].size(), 1u);
	EXPECT_TRUE(m_List.Groups()[Team][0].IsPinned());
	ASSERT_EQ(m_List.Groups()[Offline].size(), 1u);
	EXPECT_STREQ(m_List.Groups()[Offline][0].Name(), "Alice");
}

TEST_F(CQmBrowserFriendPinTest, IgnoreClanUsesSavedPinForOnlineNameMatch)
{
	m_Friends.AddFriend("Zoe", "OldClan");
	ASSERT_TRUE(m_Friends.SetFriendPinned("Zoe", "OldClan", true));
	g_Config.m_ClFriendsIgnoreClan = 1;
	m_Browser.SetClients(m_Friends, {Client("Zoe", "NewClan")});
	Update();

	const auto &Items = m_List.Groups()[m_Friends.FindCategory(IFriends::DEFAULT_CATEGORY)];
	ASSERT_EQ(Items.size(), 1u);
	EXPECT_STREQ(Items[0].Clan(), "NewClan");
	EXPECT_TRUE(Items[0].IsPinned());
	EXPECT_TRUE(m_List.Groups()[m_Friends.FindCategory(IFriends::OFFLINE_CATEGORY)].empty());
}

TEST_F(CQmBrowserFriendPinTest, ClanMatchesRemainUnpinned)
{
	m_Friends.AddFriend("", "Guild");
	EXPECT_FALSE(m_Friends.SetFriendPinned("", "Guild", true));
	m_Browser.SetClients(m_Friends, {Client("Member", "Guild")});
	Update();

	const auto &Items = m_List.Groups()[m_Friends.FindCategory(IFriends::CLAN_MEMBERS_CATEGORY)];
	ASSERT_EQ(Items.size(), 1u);
	EXPECT_EQ(Items[0].FriendState(), IFriends::FRIEND_CLAN);
	EXPECT_FALSE(Items[0].IsPinned());
}
