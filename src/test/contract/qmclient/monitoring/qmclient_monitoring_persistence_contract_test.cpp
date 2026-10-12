// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <gtest/gtest.h>
#include <test/support/qmclient_source_contract_test.h>

#include <string>

// 这些路径挂在完整客户端生命周期上，无法用稳定运行时接口观察，
// 只能按本文件既有的源码契约方式钉住关键接线。
TEST(QmStatisticsPersistence, SavesLocalStatsWithinSessionWithBoundedEntries)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/qmclient.h");
	const std::string GameClientSource = ReadRepoFile("src/game/client/gameclient.cpp");
	const std::string OnUpdate = ExtractSourceFunctionBody(QmClient, "void CQmClient::OnUpdate()");
	const std::string RecordFinish = ExtractSourceFunctionBody(QmClient, "void CQmClient::RecordQmClientLocalMapFinish(const char *pGameMode, int Score)");
	const std::string Accumulate = ExtractSourceFunctionBody(QmClient, "void CQmClient::AccumulateQmClientLocalModePlaytime(int64_t Now)");
	const std::string Load = ExtractSourceFunctionBody(QmClient, "void CQmClient::LoadQmClientLocalModeStats()");
	ASSERT_FALSE(OnUpdate.empty());
	ASSERT_FALSE(RecordFinish.empty());
	ASSERT_FALSE(Accumulate.empty());

	// 本地统计置脏 + 节流落盘：Record/Accumulate 置脏，OnUpdate 消费。
	EXPECT_NE(RecordFinish.find("m_QmStatisticsLocalStatsDirty = true"), std::string::npos);
	EXPECT_NE(Accumulate.find("m_QmStatisticsLocalStatsDirty = true"), std::string::npos);
	EXPECT_NE(OnUpdate.find("m_QmStatisticsLocalStatsDirty"), std::string::npos);
	EXPECT_NE(OnUpdate.find("m_QmStatisticsLocalSaveDueTick"), std::string::npos);

	// 条目上限同时约束运行时新增与文件加载。
	EXPECT_NE(QmClient.find("QMCLIENT_MAX_LOCAL_MODE_STATS = 256"), std::string::npos);
	EXPECT_NE(RecordFinish.find("QMCLIENT_MAX_LOCAL_MODE_STATS"), std::string::npos);
	EXPECT_NE(Accumulate.find("QMCLIENT_MAX_LOCAL_MODE_STATS"), std::string::npos);
	EXPECT_NE(Load.find("QMCLIENT_MAX_LOCAL_MODE_STATS"), std::string::npos);

	// 完成检测迁移到消息通道（0.7 RaceFinish 事件 + 0.6 finished-in 聊天广播），
	// 旧 GAMEOVER 路径必须从 gameclient.cpp 移除，避免残留死挂点。
	EXPECT_NE(QmClient.find("NETMSGTYPE_SV_RACEFINISH"), std::string::npos);
	EXPECT_NE(QmClient.find("TimeFromFinishMessage"), std::string::npos);
	EXPECT_EQ(GameClientSource.find("RecordQmClientLocalMapFinish"), std::string::npos);

	// DDNet 档案查询成功一次后，行内不再永久显示加载中。
	EXPECT_NE(QmClient.find("m_QmDdnetStatsSucceededOnce = true"), std::string::npos);
	EXPECT_NE(Header.find("QmDdnetStatsSucceededOnce"), std::string::npos);
	EXPECT_NE(Header.find("QmStatisticsFileInvalid"), std::string::npos);
}
