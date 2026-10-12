#include <game/client/components/qmclient/word_filter_list.h>

#include <gtest/gtest.h>

TEST(QmWordFilterList, ReadsPlainAndQuotedWordsWithBomAndCrLf)
{
	std::vector<std::string> vWords;
	std::string Error;
	ASSERT_TRUE(QmWordFilterList::ParseFile("\xef\xbb\xbf# comment\r\n 测试词 \r\n\"其他词\",\r\n\"测试词\"\r\n", vWords, Error));
	EXPECT_EQ(vWords, (std::vector<std::string>{"测试词", "其他词"}));
	EXPECT_TRUE(Error.empty());
}

TEST(QmWordFilterList, PreservesRegexBackslashesAndQuotedCommas)
{
	std::vector<std::string> vWords;
	std::string Error;
	ASSERT_TRUE(QmWordFilterList::ParseFile("\"spam\\d+\",\n\"a,b\",\n\"a\\\"b\"\n", vWords, Error));
	EXPECT_EQ(vWords, (std::vector<std::string>{"spam\\d+", "a,b", "a\"b"}));
}

TEST(QmWordFilterList, LoadsAListLargerThanTheOldInlineLimit)
{
	std::string Content;
	for(int Index = 0; Index < 2009; ++Index)
		Content += "\"测试词" + std::to_string(Index) + "\",\n";
	std::vector<std::string> vWords;
	std::string Error;
	ASSERT_TRUE(QmWordFilterList::ParseFile(Content, vWords, Error));
	ASSERT_EQ(vWords.size(), 2009u);
	EXPECT_EQ(vWords.back(), "测试词2008");
}

TEST(QmWordFilterList, MalformedFileDoesNotPublishPartialWords)
{
	QmWordFilterList::CFileList File;
	ASSERT_TRUE(File.UpdateFromText("原有词"));
	const uint64_t Revision = File.Revision();
	EXPECT_FALSE(File.UpdateFromText("新增词\n\"未闭合"));
	EXPECT_EQ(File.Words(), (std::vector<std::string>{"原有词"}));
	EXPECT_EQ(File.Revision(), Revision);
	EXPECT_FALSE(File.Error().empty());
	ASSERT_TRUE(File.UpdateFromText("原有词"));
	EXPECT_TRUE(File.Error().empty());
	EXPECT_EQ(File.Revision(), Revision);
}

TEST(QmWordFilterList, RejectsInvalidUtf8NulAndControlCharacters)
{
	std::vector<std::string> vWords{"保留词"};
	std::string Error;
	for(const std::string &Content : {std::string("\xff"), std::string("a\0b", 3), std::string("a\tb")})
	{
		SCOPED_TRACE(Content);
		EXPECT_FALSE(QmWordFilterList::ParseFile(Content, vWords, Error));
		EXPECT_EQ(vWords, (std::vector<std::string>{"保留词"}));
	}
}

TEST(QmWordFilterList, RejectsOversizedFilesWithoutReplacingPreviousWords)
{
	QmWordFilterList::CFileList File;
	ASSERT_TRUE(File.UpdateFromText("保留词"));
	EXPECT_FALSE(File.UpdateFromText(std::string(QmWordFilterList::MAX_FILE_BYTES + 1, 'a')));
	EXPECT_EQ(File.Words(), (std::vector<std::string>{"保留词"}));
}

TEST(QmWordFilterList, ClearingContentRemovesExternalRulesAndAdvancesRevision)
{
	QmWordFilterList::CFileList File;
	ASSERT_TRUE(File.UpdateFromText("测试词"));
	const uint64_t Revision = File.Revision();
	ASSERT_TRUE(File.UpdateFromText(""));
	EXPECT_TRUE(File.Words().empty());
	EXPECT_GT(File.Revision(), Revision);
}

TEST(QmWordFilterList, AppendRejectsOverflowAndInvalidChunksAtomically)
{
	char aList[10] = "abc";
	EXPECT_FALSE(QmWordFilterList::AppendInlineChunk(aList, sizeof(aList), "1234567"));
	EXPECT_STREQ(aList, "abc");
	EXPECT_FALSE(QmWordFilterList::AppendInlineHexChunk(aList, sizeof(aList), "00"));
	EXPECT_FALSE(QmWordFilterList::AppendInlineHexChunk(aList, sizeof(aList), "ff"));
	EXPECT_FALSE(QmWordFilterList::AppendInlineHexChunk(aList, sizeof(aList), "xyz"));
	EXPECT_STREQ(aList, "abc");
	ASSERT_TRUE(QmWordFilterList::AppendInlineHexChunk(aList, sizeof(aList), "2c646566"));
	EXPECT_STREQ(aList, "abc,def");
}
