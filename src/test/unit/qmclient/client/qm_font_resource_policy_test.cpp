#include <engine/client/qm_font_resource_policy.h>
#include <engine/client/qm_system_fonts.h>

#include <gtest/gtest.h>

TEST(QmFontResourcePolicy, ResolvesIconStyleToConfiguredThenCurrentThenRegular)
{
	int Regular = 1;
	int Current = 2;
	int Candidate = 3;

	EXPECT_EQ(ResolveFontFaceWithFallback(&Candidate, &Current, &Regular), &Candidate);
	EXPECT_EQ(ResolveFontFaceWithFallback<int *>(nullptr, &Current, &Regular), &Current);
	EXPECT_EQ(ResolveFontFaceWithFallback<int *>(nullptr, nullptr, &Regular), &Regular);
}

TEST(QmSystemFontPath, ResolvesInstalledFileNameInItsInstallationDirectory)
{
	EXPECT_EQ(QmResolveSystemFontPath("arial.ttf", "C:/Windows/Fonts"), "C:/Windows/Fonts/arial.ttf");
	EXPECT_EQ(QmResolveSystemFontPath("字体.OTF", "C:/Users/测试/Fonts"), "C:/Users/测试/Fonts/字体.OTF");
	EXPECT_EQ(QmResolveSystemFontPath("D:\\Fonts\\collection.TTC", "C:/Windows/Fonts"), "D:/Fonts/collection.TTC");
	EXPECT_EQ(QmResolveSystemFontPath("D:/Fonts/collection.otc", ""), "D:/Fonts/collection.otc");
}

TEST(QmSystemFontPath, RejectsUnsupportedOrUnresolvableInstallationRecords)
{
	for(const char *pPath : {"", "font.fon", "font.exe", "font.ttf.bak", "relative/font.ttf", "C:font.ttf", "//server/share/font.ttf"})
	{
		SCOPED_TRACE(pPath);
		EXPECT_TRUE(QmResolveSystemFontPath(pPath, "C:/Windows/Fonts").empty());
	}
	EXPECT_TRUE(QmResolveSystemFontPath("font.ttf", "").empty());
}
