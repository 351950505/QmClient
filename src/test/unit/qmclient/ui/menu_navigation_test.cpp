#include <game/client/QmUi/MenuNavigation.h>

#include <gtest/gtest.h>

TEST(MenuNavigation, EveryCommunityAndUtilityStaysAccessibleInNarrowAndScaledViews)
{
	for(const float Width : {320.0f, 640.0f, 1067.0f})
	{
		for(const float Scale : {0.75f, 1.0f, 1.5f})
		{
			SCOPED_TRACE(Width);
			SCOPED_TRACE(Scale);
			const CUIRect Bounds{8.0f, 8.0f, Width, 40.0f};
			const auto Layout = qm_navigation::Resolve(Bounds, 10, 6, 21.0f, Scale);
			ASSERT_EQ(Layout.m_LeftCount, 10);
			ASSERT_EQ(Layout.m_RightCount, 6);
			for(int Index = 0; Index < Layout.m_LeftCount; ++Index)
			{
				const auto &Slot = Layout.m_aLeft[Index];
				EXPECT_GE(Slot.x, Bounds.x);
				EXPECT_GT(Slot.w, 0.0f);
				EXPECT_LE(Slot.y + Slot.h, Bounds.y + Bounds.h);
				EXPECT_FLOAT_EQ(Slot.w, Layout.m_aRight[0].w);
				EXPECT_FLOAT_EQ(Slot.h, Layout.m_aRight[0].h);
				if(Index > 0)
					EXPECT_LE(Layout.m_aLeft[Index - 1].x + Layout.m_aLeft[Index - 1].w, Slot.x);
			}
			EXPECT_LE(Layout.m_aLeft[9].x + Layout.m_aLeft[9].w, Layout.m_aRight[0].x);
			for(int Index = 0; Index < Layout.m_RightCount; ++Index)
			{
				const auto &Slot = Layout.m_aRight[Index];
				EXPECT_LE(Slot.x + Slot.w, Bounds.x + Bounds.w + 0.001f);
				if(Index > 0)
					EXPECT_LE(Layout.m_aRight[Index - 1].x + Layout.m_aRight[Index - 1].w, Slot.x + 0.001f);
			}
		}
	}
}

TEST(MenuNavigation, CommunityCountChangesKeepTheUtilityGroupAtTheRightEdge)
{
	const CUIRect Bounds{12.0f, 4.0f, 800.0f, 40.0f};
	for(int Communities = 0; Communities <= 5; ++Communities)
	{
		const auto Layout = qm_navigation::Resolve(Bounds, 5 + Communities, 6, 21.0f, 1.5f);
		EXPECT_EQ(Layout.m_LeftCount, 5 + Communities);
		const auto &Last = Layout.m_aRight[5];
		EXPECT_NEAR(Last.x + Last.w, Bounds.x + Bounds.w, 0.001f);
		EXPECT_LE(Layout.m_aLeft[Layout.m_LeftCount - 1].x + Last.w, Layout.m_aRight[0].x);
	}
}

TEST(MenuNavigation, IndependentNavigationScaleChangesBothGroupsTogether)
{
	const CUIRect Bounds{0.0f, 0.0f, 2000.0f, 40.0f};
	const auto Small = qm_navigation::Resolve(Bounds, 5, 6, 21.0f, 0.75f);
	const auto Large = qm_navigation::Resolve(Bounds, 5, 6, 21.0f, 1.5f);
	EXPECT_FLOAT_EQ(Large.m_aLeft[0].w, Small.m_aLeft[0].w * 2.0f);
	EXPECT_FLOAT_EQ(Large.m_aRight[0].h, Small.m_aRight[0].h * 2.0f);
}

TEST(MenuNavigation, ZeroWidthDoesNotProduceNegativeOrOverlappingSlots)
{
	const auto Layout = qm_navigation::Resolve({8.0f, 8.0f, 0.0f, 40.0f}, 10, 6, 21.0f, 1.0f);
	for(const auto &Slot : Layout.m_aLeft)
	{
		EXPECT_FLOAT_EQ(Slot.x, 8.0f);
		EXPECT_FLOAT_EQ(Slot.w, 0.0f);
	}
	for(const auto &Slot : Layout.m_aRight)
	{
		EXPECT_FLOAT_EQ(Slot.x, 8.0f);
		EXPECT_FLOAT_EQ(Slot.w, 0.0f);
	}
}
