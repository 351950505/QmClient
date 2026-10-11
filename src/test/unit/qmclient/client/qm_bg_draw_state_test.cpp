#include <game/client/components/qmclient/bg_draw_state.h>

#include <gtest/gtest.h>

namespace
{
	CBgDrawItemDataPoint Point(float X, float Y, float Width = 2.0f)
	{
		return {X, Y, Width, 1.0f, 0.5f, 0.0f, 1.0f};
	}
}

TEST(QmBgDrawStroke, LoadingPreservesBothEndpointsAndSavedWidths)
{
	const CQmBgDrawStroke Stroke(CBgDrawItemData{Point(10, 20, 3), Point(100, -30, 8)});
	ASSERT_EQ(Stroke.Data().size(), 2u);
	EXPECT_FLOAT_EQ(Stroke.Data().back().x, 100);
	EXPECT_FLOAT_EQ(Stroke.Data().back().y, -30);
	EXPECT_FLOAT_EQ(Stroke.Data().back().w, 8);
	EXPECT_EQ(Stroke.Min(), vec2(7, -38));
	EXPECT_EQ(Stroke.Max(), vec2(108, 23));
}

TEST(QmBgDrawStroke, LoadingRetainsIntermediatePoints)
{
	const CQmBgDrawStroke Stroke(CBgDrawItemData{Point(0, 0), Point(1, 1), Point(2, 2)});
	ASSERT_EQ(Stroke.Data().size(), 3u);
	EXPECT_FLOAT_EQ(Stroke.Data()[1].x, 1);
	EXPECT_FLOAT_EQ(Stroke.Data()[2].x, 2);
}

TEST(QmBgDrawStroke, MovingExpandsVisibleBounds)
{
	CQmBgDrawStroke Stroke(Point(10, 20));
	ASSERT_TRUE(Stroke.MoveTo(Point(200, -100, 5)));
	EXPECT_EQ(Stroke.Min(), vec2(8, -105));
	EXPECT_EQ(Stroke.Max(), vec2(205, 22));
}

TEST(QmBgDrawStroke, UnchangedPointDoesNotRequestGeometryRebuild)
{
	CQmBgDrawStroke Stroke(Point(10, 20));
	EXPECT_FALSE(Stroke.MoveTo(Point(10, 20)));
	EXPECT_TRUE(Stroke.MoveTo(Point(10, 20, 4)));
}

TEST(QmBgDrawStroke, PenUpAndLoadedStrokesRejectFurtherDrawing)
{
	CQmBgDrawStroke Stroke(Point(10, 20));
	Stroke.PenUp();
	Stroke.PenUp();
	EXPECT_FALSE(Stroke.MoveTo(Point(100, 200)));
	CQmBgDrawStroke Loaded(CBgDrawItemData{Point(0, 0)});
	EXPECT_FALSE(Loaded.MoveTo(Point(100, 200)));
}

TEST(QmBgDrawStroke, StopsAtPointLimit)
{
	CQmBgDrawStroke Stroke(Point(0, 0));
	for(size_t i = 1; i < CQmBgDrawStroke::MAX_POINTS; ++i)
		ASSERT_TRUE(Stroke.MoveTo(Point(static_cast<float>(i) * 10.0f, 0)));
	EXPECT_FALSE(Stroke.MoveTo(Point(20000, 0)));
	EXPECT_EQ(Stroke.Data().size(), CQmBgDrawStroke::MAX_POINTS);
}

TEST(QmBgDrawStore, ZeroCapacityReleasesEveryActiveBorrow)
{
	std::list<int> Items{10, 20};
	std::array<int *, 2> apActive{&Items.front(), &Items.back()};
	EXPECT_TRUE(QmBgDrawMakeSpace(Items, apActive, 0, 1));
	EXPECT_TRUE(Items.empty());
	EXPECT_EQ(apActive[0], nullptr);
	EXPECT_EQ(apActive[1], nullptr);
	EXPECT_FALSE(QmBgDrawMakeSpace(Items, apActive, 0, 1));
}

TEST(QmBgDrawStore, WholeBatchReplacementReleasesEveryActiveBorrow)
{
	std::list<int> Items{10, 20};
	std::array<int *, 2> apActive{&Items.front(), &Items.back()};
	EXPECT_TRUE(QmBgDrawMakeSpace(Items, apActive, 2, 2));
	EXPECT_TRUE(Items.empty());
	EXPECT_EQ(apActive[0], nullptr);
	EXPECT_EQ(apActive[1], nullptr);
}

TEST(QmBgDrawStore, TrimmingPreservesBorrowOfRetainedStroke)
{
	std::list<int> Items{10, 20, 30};
	int *pRetained = &Items.back();
	std::array<int *, 2> apActive{&Items.front(), pRetained};
	EXPECT_TRUE(QmBgDrawMakeSpace(Items, apActive, 2, 1));
	ASSERT_EQ(Items.size(), 1u);
	EXPECT_EQ(Items.front(), 30);
	EXPECT_EQ(apActive[0], nullptr);
	EXPECT_EQ(apActive[1], pRetained);
}
