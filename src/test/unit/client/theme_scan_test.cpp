#include <game/client/components/theme_scan.h>

#include <gtest/gtest.h>

TEST(ThemeScan, FileCandidateDetection)
{
	EXPECT_TRUE(IsThemeFileCandidate("heavens.map"));
	EXPECT_TRUE(IsThemeFileCandidate("HEAVENS.MAP"));
	for(const char *pName : {"heavens.png", "heavens.webp", "heavens.jpg", "heavens.mp4", "heavens.webm", "none.png", "auto.png", "rand.png", "heavens.txt", "themes", ".map", ""})
		EXPECT_FALSE(IsThemeFileCandidate(pName)) << pName;
	EXPECT_FALSE(IsThemeFileCandidate(nullptr));
}

TEST(ThemeScan, CandidatePathBuilding)
{
	EXPECT_EQ(BuildThemeCandidatePath("themes/autumn_night", ".map"), "themes/autumn_night.map");
	EXPECT_EQ(BuildThemeCandidatePath("themes/jungle_day", ".png"), "themes/jungle_day.png");
}

TEST(ThemeScan, IconPathMapping)
{
	EXPECT_EQ(ThemeIconPathFromName(""), "themes/none.png");
	EXPECT_EQ(ThemeIconPathFromName("auto"), "themes/auto.png");
	EXPECT_EQ(ThemeIconPathFromName("rand"), "themes/rand.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.map"), "themes/autumn.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn_day.map"), "themes/autumn.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn_night.map"), "themes/autumn.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.png"), "themes/autumn.png");
	EXPECT_EQ(ThemeIconPathFromName("AUTUMN.PNG"), "themes/AUTUMN.PNG");
	EXPECT_EQ(ThemeIconPathFromName("autumn.webp"), "themes/autumn.webp");
#if defined(CONF_VIDEORECORDER)
	EXPECT_EQ(ThemeIconPathFromName("autumn.jpg"), "themes/autumn.jpg");
	EXPECT_EQ(ThemeIconPathFromName("autumn.jpeg"), "themes/autumn.jpeg");
	EXPECT_EQ(ThemeIconPathFromName("autumn.jfif"), "themes/autumn.jfif");
	EXPECT_EQ(ThemeIconPathFromName("autumn.bmp"), "themes/autumn.bmp");
	EXPECT_EQ(ThemeIconPathFromName("autumn.tga"), "themes/autumn.tga");
	EXPECT_EQ(ThemeIconPathFromName("autumn.tiff"), "themes/autumn.tiff");
	EXPECT_EQ(ThemeIconPathFromName("autumn.gif"), "themes/autumn.gif");
#else
	EXPECT_EQ(ThemeIconPathFromName("autumn.jpg"), "themes/autumn.jpg.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.jpeg"), "themes/autumn.jpeg.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.jfif"), "themes/autumn.jfif.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.bmp"), "themes/autumn.bmp.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.tga"), "themes/autumn.tga.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.tiff"), "themes/autumn.tiff.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn.gif"), "themes/autumn.gif.png");
#endif
	EXPECT_EQ(ThemeIconPathFromName("autumn_day.webp"), "themes/autumn_day.webp");
#if defined(CONF_VIDEORECORDER)
	EXPECT_EQ(ThemeIconPathFromName("autumn_day.jpg"), "themes/autumn_day.jpg");
#else
	EXPECT_EQ(ThemeIconPathFromName("autumn_day.jpg"), "themes/autumn_day.jpg.png");
#endif
	EXPECT_EQ(ThemeIconPathFromName("autumn.mp4"), "themes/autumn.png");
	EXPECT_EQ(ThemeIconPathFromName("autumn_day.mp4"), "themes/autumn.png");
}

TEST(ThemeScan, PreviewOnlyDirectoryDoesNotProduceSelectableThemes)
{
	std::vector<SThemeFileEntry> Entries;
	AppendThemeFileEntry(Entries, "none.png", false);
	AppendThemeFileEntry(Entries, "auto.png", false);
	AppendThemeFileEntry(Entries, "rand.png", false);
	AppendThemeFileEntry(Entries, "autumn.png", false);
	AppendThemeFileEntry(Entries, "folder.map", true);
	EXPECT_TRUE(Entries.empty());
}

TEST(ThemeScan, DayAndNightMapsMergeWithoutPreviewDuplicates)
{
	std::vector<SThemeFileEntry> Entries;
	AppendThemeFileEntry(Entries, "autumn_night.map", false);
	ASSERT_EQ(Entries.size(), 1u);
	EXPECT_FALSE(Entries[0].m_HasDay);
	EXPECT_TRUE(Entries[0].m_HasNight);
	AppendThemeFileEntry(Entries, "autumn_day.map", false);
	AppendThemeFileEntry(Entries, "autumn.png", false);
	AppendThemeFileEntry(Entries, "autumn_day.map", false);
	ASSERT_EQ(Entries.size(), 1u);
	EXPECT_EQ(Entries[0].m_Name, "autumn");
	EXPECT_TRUE(Entries[0].m_HasDay);
	EXPECT_TRUE(Entries[0].m_HasNight);
	EXPECT_EQ(Entries[0].m_IconPath, "themes/autumn.png");
	EXPECT_EQ(ResolveThemeMapFileName(Entries[0].m_aMapNames, true), "autumn_day.map");
	EXPECT_EQ(ResolveThemeMapFileName(Entries[0].m_aMapNames, false), "autumn_night.map");
}

TEST(ThemeScan, ExactCasePathsRemainLoadableAndMissingVariantFallsBack)
{
	std::vector<SThemeFileEntry> Entries;
	AppendThemeFileEntry(Entries, "Desert_DAY.MAP", false);
	ASSERT_EQ(Entries.size(), 1u);
	EXPECT_EQ(Entries[0].m_Name, "Desert");
	EXPECT_EQ(ResolveThemeMapFileName(Entries[0].m_aMapNames, false), "Desert_DAY.MAP");
	AppendThemeFileEntry(Entries, "Desert.MAP", false);
	EXPECT_EQ(ResolveThemeMapFileName(Entries[0].m_aMapNames, false), "Desert.MAP");
	EXPECT_EQ(ResolveThemeMapFileName(Entries[0].m_aMapNames, true), "Desert_DAY.MAP");
}

TEST(ThemeScan, FreshScanDropsRemovedVariantsAndPreservesOnlyExistingPaths)
{
	std::vector<SThemeFileEntry> Before;
	AppendThemeFileEntry(Before, "autumn_day.map", false);
	AppendThemeFileEntry(Before, "autumn_night.map", false);
	std::vector<SThemeFileEntry> Refreshed;
	AppendThemeFileEntry(Refreshed, "autumn_night.map", false);
	ASSERT_EQ(Refreshed.size(), 1u);
	EXPECT_FALSE(Refreshed[0].m_HasDay);
	EXPECT_TRUE(Refreshed[0].m_HasNight);
	EXPECT_EQ(ResolveThemeMapFileName(Refreshed[0].m_aMapNames, true), "autumn_night.map");
	EXPECT_TRUE(Before[0].m_HasDay);
}

TEST(ThemeScan, DisplayNamesPreserveBuiltinsAndRemoveMapVariantSuffixes)
{
	EXPECT_EQ(ThemeDisplayName(""), "(none)");
	EXPECT_EQ(ThemeDisplayName("auto"), "(seasons)");
	EXPECT_EQ(ThemeDisplayName("rand"), "(random)");
	EXPECT_EQ(ThemeDisplayName("autumn_day.map"), "autumn");
	EXPECT_EQ(ThemeDisplayName("Desert_NIGHT.MAP"), "Desert");
	EXPECT_EQ(ThemeDisplayName("My (theme)"), "My (theme)");
}

TEST(ThemeScan, DisplayDoesNotNormalizeAnAlreadyMergedNameAgain)
{
	std::vector<SThemeFileEntry> Entries;
	AppendThemeFileEntry(Entries, "foo_day_day.map", false);
	ASSERT_EQ(Entries.size(), 1u);
	EXPECT_EQ(Entries[0].m_Name, "foo_day");
	EXPECT_EQ(ThemeDisplayName(Entries[0].m_Name.c_str()), "foo_day");
	EXPECT_EQ(ThemeDisplayName("foo_day_day.map"), "foo_day");
	EXPECT_EQ(ThemeDisplayName("custom_night"), "custom_night");
}
