#include <game/client/QmUi/SettingsSearchIndex.h>

#include <gtest/gtest.h>

#include <utility>

namespace
{
	using namespace qm_settings_search;

	SDocument Setting(const char *pCard, const char *pLabel, const char *pConfig = "")
	{
		SDocument Document;
		Document.m_CardId = pCard;
		Document.m_SettingId = pConfig;
		Document.m_Label = pLabel;
		Document.m_ConfigName = pConfig;
		return Document;
	}
} // namespace

TEST(QmSettingsSearchIndex, ExactLabelPrecedesRepeatedDescriptionMentions)
{
	CIndex Index;
	SDocument Description = Setting("effects", "Effects");
	Description.m_Description = "opacity opacity opacity opacity opacity";
	Index.SetDocuments({Description, Setting("chat", "Opacity")});
	const auto vMatches = Index.Search("opacity");
	ASSERT_EQ(vMatches.size(), 2u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 1u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::EXACT);
}

TEST(QmSettingsSearchIndex, ExactConfigurationPrecedesCardKeywords)
{
	CIndex Index;
	SDocument Other = Setting("other", "Other");
	Other.m_Keywords = "qm_chat_opacity";
	Index.SetDocuments({Other, Setting("chat", "Background", "qm_chat_opacity")});
	const auto vMatches = Index.Search("qm_chat_opacity");
	ASSERT_EQ(vMatches.size(), 2u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 1u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::EXACT);
}

TEST(QmSettingsSearchIndex, ConfigurationWithSettingLabelPrecedesBarePreview)
{
	CIndex Index;
	Index.SetDocuments({Setting("preview", "", "cl_chat_background_color"), Setting("chat", "Background color", "cl_chat_background_color")});
	const auto vMatches = Index.Search("cl_chat_background_color");
	ASSERT_EQ(vMatches.size(), 2u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 1u);
}

TEST(QmSettingsSearchIndex, AllQueryWordsMustBelongToOneSettingAndItsContext)
{
	CIndex Index;
	SDocument ChatOpacity = Setting("chat", "Opacity");
	ChatOpacity.m_CardTitle = "Chat";
	SDocument ChatFont = Setting("chat", "Font");
	ChatFont.m_CardTitle = "Chat";
	Index.SetDocuments({ChatFont, Setting("other", "Opacity"), ChatOpacity});
	const auto vMatches = Index.Search("chat opacity");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 2u);
}

TEST(QmSettingsSearchIndex, QueryWordOrderDoesNotChangeRelevantSetting)
{
	CIndex Index;
	Index.SetDocuments({Setting("chat", "Chat background opacity"), Setting("chat", "Chat font size")});
	const auto vForward = Index.Search("chat opacity");
	const auto vReversed = Index.Search("opacity chat");
	ASSERT_EQ(vForward.size(), 1u);
	ASSERT_EQ(vReversed.size(), 1u);
	EXPECT_EQ(vForward.front().m_DocumentIndex, vReversed.front().m_DocumentIndex);
}

TEST(QmSettingsSearchIndex, UnspacedChineseCanCombineCardAndSettingTerms)
{
	CIndex Index;
	SDocument Opacity = Setting("chat", "透明度");
	Opacity.m_CardTitle = "聊天";
	Index.SetDocuments({Opacity, Setting("other", "透明度")});
	const auto vMatches = Index.Search("聊天透明度");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
}

TEST(QmSettingsSearchIndex, ChinesePartialPhraseFindsLongerLabel)
{
	CIndex Index;
	Index.SetDocuments({Setting("chat", "背景透明度"), Setting("chat", "字体颜色")});
	const auto vMatches = Index.Search("透明");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
}

TEST(QmSettingsSearchIndex, ChineseSegmentationDoesNotDropAnUnmatchedCharacter)
{
	CIndex Index;
	SDocument Opacity = Setting("chat", "透明度");
	Opacity.m_CardTitle = "聊天";
	Index.SetDocuments({Opacity});
	EXPECT_TRUE(Index.Search("聊天错透明度").empty());
}

TEST(QmSettingsSearchIndex, LongChineseQueryStillRequiresEveryCharacterToMatch)
{
	CIndex Index;
	SDocument Opacity = Setting("chat", "透明度");
	Opacity.m_CardTitle = "聊天";
	Index.SetDocuments({Opacity});
	std::string Prefix;
	for(int Repeat = 0; Repeat < 18; ++Repeat)
		Prefix += "聊天";
	// 接近界面的 128 字节输入上限，不能裁掉后面的未匹配内容。
	EXPECT_EQ(Index.Search((Prefix + "透明度").c_str()).size(), 1u);
	EXPECT_TRUE(Index.Search((Prefix + "噪声透明度").c_str()).empty());
}

TEST(QmSettingsSearchIndex, MixedChineseAndEnglishUsesBothTerms)
{
	CIndex Index;
	SDocument Hud = Setting("hud", "透明度");
	Hud.m_CardTitle = "HUD";
	Index.SetDocuments({Hud, Setting("chat", "透明度")});
	const auto vMatches = Index.Search("HUD透明度");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
}

TEST(QmSettingsSearchIndex, CommonSpokenSynonymFindsEquivalentSetting)
{
	CIndex Index;
	SDocument Font = Setting("chat", "文字大小");
	Font.m_CardTitle = "Chat";
	Index.SetDocuments({Font});
	const auto vMatches = Index.Search("聊天 字号");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
}

TEST(QmSettingsSearchIndex, MultiwordEnglishSynonymFindsChineseLabel)
{
	CIndex Index;
	Index.SetDocuments({Setting("text", "字号")});
	EXPECT_EQ(Index.Search("font size").size(), 1u);
}

TEST(QmSettingsSearchIndex, ExplicitAliasesAndPinyinKeywordsRemainSearchable)
{
	CIndex Index;
	SDocument Outline = Setting("skin", "Outline");
	Outline.m_Aliases = "皮肤轮廓";
	Outline.m_Keywords = "miaobian";
	Index.SetDocuments({Outline});
	for(const char *pQuery : {"皮肤轮廓", "miaobian"})
	{
		SCOPED_TRACE(pQuery);
		EXPECT_EQ(Index.Search(pQuery).size(), 1u);
	}
}

TEST(QmSettingsSearchIndex, CommonChinesePhraseSupportsFullPinyinAndInitials)
{
	CIndex Index;
	Index.SetDocuments({Setting("island", "灵动岛")});
	for(const char *pQuery : {"lingdongdao", "ldd", "lingdong"})
	{
		SCOPED_TRACE(pQuery);
		const auto vMatches = Index.Search(pQuery);
		ASSERT_EQ(vMatches.size(), 1u);
		EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
	}
}

TEST(QmSettingsSearchIndex, AdjacentKnownChineseWordsSupportCombinedPinyin)
{
	CIndex Index;
	Index.SetDocuments({Setting("chat", "聊天透明度")});
	for(const char *pQuery : {"liaotiantoumingdu", "lttmd", "toumingdu"})
	{
		SCOPED_TRACE(pQuery);
		EXPECT_EQ(Index.Search(pQuery).size(), 1u);
	}
}

TEST(QmSettingsSearchIndex, EnglishTranspositionIsFuzzyMatch)
{
	CIndex Index;
	Index.SetDocuments({Setting("scoreboard", "Scoreboard"), Setting("sound", "Sound")});
	const auto vMatches = Index.Search("socreboard");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 0u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::FUZZY);
}

TEST(QmSettingsSearchIndex, PinyinMissingLetterIsFuzzyMatch)
{
	CIndex Index;
	Index.SetDocuments({Setting("island", "灵动岛")});
	const auto vMatches = Index.Search("lingdongdo");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::FUZZY);
}

TEST(QmSettingsSearchIndex, FuzzyDistanceWorkspaceDoesNotLeakPreviousCandidateRows)
{
	CIndex Index;
	Index.SetDocuments({Setting("unrelated", "Scorexxxxd"), Setting("scoreboard", "Scoreboard")});
	for(const char *pQuery : {"socreboard", "scoreboad", "socreboard"})
	{
		SCOPED_TRACE(pQuery);
		const auto vMatches = Index.Search(pQuery);
		ASSERT_EQ(vMatches.size(), 1u);
		EXPECT_EQ(vMatches.front().m_DocumentIndex, 1u);
		EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::FUZZY);
	}
}

TEST(QmSettingsSearchIndex, AccurateResultsPrecedeFuzzyResults)
{
	CIndex Index;
	SDocument ExactAlias = Setting("accurate", "Results");
	ExactAlias.m_Aliases = "scoreboard";
	Index.SetDocuments({Setting("fuzzy", "Scorebroad"), ExactAlias});
	const auto vMatches = Index.Search("scoreboard");
	ASSERT_EQ(vMatches.size(), 2u);
	EXPECT_EQ(vMatches.front().m_DocumentIndex, 1u);
	EXPECT_EQ(vMatches.back().m_Kind, EMatchKind::FUZZY);
}

TEST(QmSettingsSearchIndex, ShortWordsDoNotUseFuzzyOrInfixMatching)
{
	CIndex Index;
	Index.SetDocuments({Setting("mouse", "Mouse"), Setting("ui", "UI")});
	EXPECT_FALSE(Index.Search("mou").empty());
	EXPECT_TRUE(Index.Search("mose").empty());
	EXPECT_TRUE(Index.Search("u").empty());
	EXPECT_TRUE(Index.Search("ou").empty());
	EXPECT_EQ(Index.Search("ui").size(), 1u);
}

TEST(QmSettingsSearchIndex, UnicodeWhitespaceAndFullWidthCaseNormalize)
{
	CIndex Index;
	Index.SetDocuments({Setting("hud", "HUD opacity")});
	const auto vMatches = Index.Search("　ＨＵＤ　ＯＰＡＣＩＴＹ　");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::EXACT);
}

TEST(QmSettingsSearchIndex, UnicodeCaseFoldingPreservesNonAsciiLabels)
{
	CIndex Index;
	Index.SetDocuments({Setting("graphics", "ÉCLAIRAGE")});
	const auto vMatches = Index.Search("éclairage");
	ASSERT_EQ(vMatches.size(), 1u);
	EXPECT_EQ(vMatches.front().m_Kind, EMatchKind::EXACT);
}

TEST(QmSettingsSearchIndex, EmptyAndWhitespaceQueriesReturnNoResults)
{
	CIndex Index;
	Index.SetDocuments({Setting("chat", "Chat")});
	const char *apQueries[] = {nullptr, "", " \t\r\n　"};
	for(const char *pQuery : apQueries)
	{
		SCOPED_TRACE(pQuery != nullptr ? pQuery : "null");
		EXPECT_TRUE(Index.Search(pQuery).empty());
	}
}

TEST(QmSettingsSearchIndex, EqualScoresPreserveDocumentOrderAcrossQueries)
{
	CIndex Index;
	Index.SetDocuments({Setting("first", "Opacity"), Setting("second", "Opacity"), Setting("third", "Opacity")});
	for(int QueryIndex = 0; QueryIndex < 3; ++QueryIndex)
	{
		const auto vMatches = Index.Search("opacity");
		ASSERT_EQ(vMatches.size(), 3u);
		for(size_t Index = 0; Index < vMatches.size(); ++Index)
			EXPECT_EQ(vMatches[Index].m_DocumentIndex, Index);
	}
}

TEST(QmSettingsSearchIndex, ReplacingDocumentsDropsPreviousTermsAndKeepsOwnedText)
{
	CIndex Index;
	Index.SetDocuments({Setting("chat", "Chat")});
	std::vector<SDocument> vDocuments = {Setting("sound", "Volume")};
	Index.SetDocuments(std::move(vDocuments));
	ASSERT_EQ(Index.Documents().size(), 1u);
	EXPECT_EQ(Index.Documents().front().m_CardId, "sound");
	EXPECT_TRUE(Index.Search("chat").empty());
	EXPECT_EQ(Index.Search("volume").size(), 1u);
	Index.SetDocuments({});
	EXPECT_TRUE(Index.Search("volume").empty());
}
