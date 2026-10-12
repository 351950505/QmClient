#include <game/client/QmUi/QmCardRegistry.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <string>

namespace
{
	class CQmSettingsSearchRanking : public ::testing::Test
	{
	protected:
		qm_card_order::CModel m_Model;

		void SetUp() override
		{
			m_Model.SetEntries(qm_card_registry::BuildDefaultEntries());
		}
	};

	const qm_card_registry::SSettingSearchMatch *FindSetting(const qm_card_registry::SCardSearchResult &Result, const char *pConfigName)
	{
		const auto It = std::find_if(Result.m_vMatchedSettings.begin(), Result.m_vMatchedSettings.end(), [&](const auto &Setting) {
			return Setting.m_ConfigName == pConfigName;
		});
		return It != Result.m_vMatchedSettings.end() ? &*It : nullptr;
	}
}

TEST_F(CQmSettingsSearchRanking, ExactConfigurationRanksItsEditableSettingBeforePreview)
{
	const auto vResults = qm_card_registry::SearchCards("cl_chat_background_color", m_Model);
	ASSERT_FALSE(vResults.empty());
	EXPECT_STREQ(vResults.front().m_pStableId, "deck:appearance-chat-settings");
	const auto *pSetting = FindSetting(vResults.front(), "cl_chat_background_color");
	ASSERT_NE(pSetting, nullptr);
	EXPECT_EQ(pSetting->m_Label, "Chat background color");
	EXPECT_FALSE(pSetting->m_DisplayLabel.empty());
	EXPECT_EQ(pSetting->m_SettingId, "cl_chat_background_color");
	EXPECT_EQ(pSetting->m_Kind, qm_settings_search::EMatchKind::EXACT);
}

TEST_F(CQmSettingsSearchRanking, ExactSettingLabelReturnsItsConfigurationAndOriginalLabel)
{
	const auto vResults = qm_card_registry::SearchCards("Chat font size", m_Model);
	ASSERT_FALSE(vResults.empty());
	EXPECT_STREQ(vResults.front().m_pStableId, "deck:appearance-chat-settings");
	const auto *pSetting = FindSetting(vResults.front(), "cl_chat_size");
	ASSERT_NE(pSetting, nullptr);
	EXPECT_EQ(pSetting->m_Label, "Chat font size");
	EXPECT_GT(pSetting->m_Score, 0.0f);
}

TEST_F(CQmSettingsSearchRanking, MultipleChineseTermsLocateTheSameSettingInEitherOrder)
{
	for(const char *pQuery : {"聊天 透明度", "透明度 聊天", "聊天透明度"})
	{
		SCOPED_TRACE(pQuery);
		const auto vResults = qm_card_registry::SearchCards(pQuery, m_Model);
		ASSERT_FALSE(vResults.empty());
		EXPECT_STREQ(vResults.front().m_pStableId, "deck:appearance-chat-settings");
		const auto *pSetting = FindSetting(vResults.front(), "cl_chat_background_color");
		ASSERT_NE(pSetting, nullptr);
		EXPECT_EQ(pSetting->m_Label, "Chat background color");
	}
}

TEST_F(CQmSettingsSearchRanking, PinyinMetadataFindsCameraWithoutOpeningItsPage)
{
	const auto vResults = qm_card_registry::SearchCards("jingtou", m_Model);
	const auto It = std::find_if(vResults.begin(), vResults.end(), [](const auto &Result) {
		return std::string(Result.m_pStableId) == "qm:camera_view";
	});
	ASSERT_NE(It, vResults.end());
	EXPECT_LT(std::distance(vResults.begin(), It), 3);
}

TEST_F(CQmSettingsSearchRanking, AliasesMergeIntoOneCardAndKeepItsSavedNavigation)
{
	m_Model.MoveToTab("deck:appearance-laser-enhanced", "function", 2, 0);
	const auto vResults = qm_card_registry::SearchCards("激光", m_Model);
	EXPECT_EQ(std::count_if(vResults.begin(), vResults.end(), [](const auto &Result) {
		return std::string(Result.m_pStableId) == "deck:appearance-laser-enhanced";
	}),
		1);
	EXPECT_TRUE(std::none_of(vResults.begin(), vResults.end(), [](const auto &Result) {
		return std::string(Result.m_pStableId) == "qm:laser";
	}));
	const auto It = std::find_if(vResults.begin(), vResults.end(), [](const auto &Result) {
		return std::string(Result.m_pStableId) == "deck:appearance-laser-enhanced";
	});
	ASSERT_NE(It, vResults.end());
	EXPECT_STREQ(It->m_Target.m_pTab, "function");
}

TEST_F(CQmSettingsSearchRanking, ConfigQueriesPreserveTheLabelsOfConsoleAndMouseControls)
{
	struct SCase
	{
		const char *m_pConfig;
		const char *m_pCard;
		const char *m_pLabel;
	};
	const SCase aCases[] = {
		{"qm_console_font_size", "qm:console", "Console font size"},
		{"inp_mousesens", "deck:controls-mouse", "Ingame mouse sens."},
		{"ui_mousesens", "deck:controls-mouse", "UI mouse sens."},
		{"inp_controller_tolerance", "deck:controls-controller", "Controller jitter tolerance"},
	};
	for(const auto &Case : aCases)
	{
		SCOPED_TRACE(Case.m_pConfig);
		const auto vResults = qm_card_registry::SearchCards(Case.m_pConfig, m_Model);
		ASSERT_FALSE(vResults.empty());
		EXPECT_STREQ(vResults.front().m_pStableId, Case.m_pCard);
		const auto *pSetting = FindSetting(vResults.front(), Case.m_pConfig);
		ASSERT_NE(pSetting, nullptr);
		EXPECT_EQ(pSetting->m_Label, Case.m_pLabel);
	}
}

TEST_F(CQmSettingsSearchRanking, TermsFromDifferentControlsDoNotBecomeOneSettingMatch)
{
	const auto vResults = qm_card_registry::SearchCards("Maximum cursor distance UI mouse sens", m_Model);
	EXPECT_TRUE(std::none_of(vResults.begin(), vResults.end(), [](const auto &Result) {
		return std::string(Result.m_pStableId) == "deck:controls-mouse";
	}));
}

TEST_F(CQmSettingsSearchRanking, EmptyAndWhitespaceQueriesHaveNoResults)
{
	const char *apQueries[] = {nullptr, "", " \t\n", "　"};
	for(const char *pQuery : apQueries)
		EXPECT_TRUE(qm_card_registry::SearchCards(pQuery, m_Model).empty());
}
