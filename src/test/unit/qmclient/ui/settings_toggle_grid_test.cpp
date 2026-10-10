#include <game/client/QmUi/SettingsToggleGrid.h>

#include <gtest/gtest.h>

TEST(SettingsToggleGrid, AvailableWidthDeterminesColumnsWithoutCardWidthCategories)
{
	std::array<SSettingsToggleSize, 11> aItems;
	aItems.fill({100.0f, 45.0f});
	const auto Wide = ResolveSettingsToggleGrid(700, aItems, 10);
	const auto Narrow = ResolveSettingsToggleGrid(340, aItems, 10);
	EXPECT_EQ(Wide.m_Columns, 6);
	EXPECT_EQ(Wide.m_Rows, 2);
	EXPECT_EQ(Narrow.m_Columns, 3);
	EXPECT_EQ(Narrow.m_Rows, 4);
}

TEST(SettingsToggleGrid, EachColumnUsesItsOwnLongestItem)
{
	const std::array<SSettingsToggleSize, 4> aItems = {{{240, 30}, {60, 30}, {220, 30}, {50, 30}}};
	const auto Grid = ResolveSettingsToggleGrid(350, aItems, 10);
	const CUIRect Area{20, 40, 350, Grid.Height()};
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 0).w, 260);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 1).w, 80);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 2).w, Grid.Cell(Area, 0).w);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 1).x - Grid.Cell(Area, 0).x - Grid.Cell(Area, 0).w, 10);
}

TEST(SettingsToggleGrid, ColumnFitChecksTheActualAssignment)
{
	const std::array<SSettingsToggleSize, 6> aItems = {{{300, 30}, {100, 30}, {100, 30}, {300, 30}, {100, 30}, {100, 30}}};
	const auto Grid = ResolveSettingsToggleGrid(520, aItems, 10);
	EXPECT_EQ(Grid.m_Columns, 3);
	EXPECT_EQ(Grid.m_Rows, 2);
}

TEST(SettingsToggleGrid, ItemHeightsDetermineEachRowAndCanBeRemeasuredAfterWrapping)
{
	std::array<SSettingsToggleSize, 5> aItems = {{{100, 30}, {100, 50}, {100, 80}, {100, 20}, {100, 40}}};
	auto Grid = ResolveSettingsToggleGrid(210, aItems, 10);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_FLOAT_EQ(Grid.Height(), 190);
	aItems[0].m_Height = 70;
	Grid.SetItemHeights(aItems);
	const CUIRect Area{20, 40, 210, Grid.Height()};
	EXPECT_FLOAT_EQ(Grid.Height(), 210);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 2).y, 120);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 4).y + Grid.Cell(Area, 4).h, Area.y + Area.h);
}

TEST(SettingsToggleGrid, ResizingReflowsTheSameItemsAndRestoresTheWideLayout)
{
	const std::array<SSettingsToggleSize, 3> aItems = {{{100, 30}, {150, 60}, {80, 40}}};
	const auto Wide = ResolveSettingsToggleGrid(350, aItems, 10);
	const auto Narrow = ResolveSettingsToggleGrid(120, aItems, 10);
	const auto Restored = ResolveSettingsToggleGrid(350, aItems, 10);
	EXPECT_EQ(Wide.m_Columns, 3);
	EXPECT_EQ(Narrow.m_Columns, 1);
	EXPECT_FLOAT_EQ(Narrow.Height(), 150);
	EXPECT_EQ(Restored.m_Columns, Wide.m_Columns);
	EXPECT_FLOAT_EQ(Restored.Height(), Wide.Height());
	EXPECT_FLOAT_EQ(Narrow.Cell({0, 0, 120, Narrow.Height()}, 1).w, 120);
}

TEST(SettingsToggleGrid, MeasuredEmptyAndTinyAreasRemainInsideTheirBounds)
{
	const auto Empty = ResolveSettingsToggleGrid(0, std::array<SSettingsToggleSize, 0>{}, 10);
	EXPECT_EQ(Empty.m_Rows, 0);
	EXPECT_FLOAT_EQ(Empty.Height(), 0);
	const std::array<SSettingsToggleSize, 2> aItems = {{{100, 40}, {200, 60}}};
	const auto Tiny = ResolveSettingsToggleGrid(5, aItems, 10);
	const CUIRect Area{10, 20, 5, Tiny.Height()};
	EXPECT_EQ(Tiny.m_Columns, 1);
	EXPECT_FLOAT_EQ(Tiny.Cell(Area, 1).w, 5);
	EXPECT_FLOAT_EQ(Tiny.Cell(Area, 1).y + Tiny.Cell(Area, 1).h, Area.y + Area.h);
}

TEST(SettingsToggleGrid, ScalingContentAndAvailableWidthPreservesTheLayout)
{
	const std::array<SSettingsToggleSize, 4> aItems = {{{120, 30}, {60, 50}, {200, 40}, {90, 20}}};
	const auto Base = ResolveSettingsToggleGrid(310, aItems, 10);
	for(const float Scale : {0.5f, 1.25f, 2.0f})
	{
		SCOPED_TRACE(Scale);
		auto aScaledItems = aItems;
		for(auto &Item : aScaledItems)
		{
			Item.m_Width *= Scale;
			Item.m_Height *= Scale;
		}
		const auto Scaled = ResolveSettingsToggleGrid(310 * Scale, aScaledItems, 10 * Scale);
		EXPECT_EQ(Scaled.m_Columns, Base.m_Columns);
		EXPECT_EQ(Scaled.m_Rows, Base.m_Rows);
		EXPECT_FLOAT_EQ(Scaled.Height(), Base.Height() * Scale);
		const CUIRect Area{0, 0, 310 * Scale, Scaled.Height()};
		for(int Index = 0; Index < static_cast<int>(aItems.size()); ++Index)
		{
			const auto Cell = Scaled.Cell(Area, Index);
			EXPECT_GE(Cell.w, aScaledItems[Index].m_Width);
			EXPECT_GE(Cell.h, aScaledItems[Index].m_Height);
			EXPECT_LE(Cell.x + Cell.w, Area.w);
			EXPECT_LE(Cell.y + Cell.h, Area.h);
		}
	}
}

TEST(SettingsToggleGrid, LongLabelsReduceColumnsWithoutReducingRequiredCellWidth)
{
	std::array<SSettingsToggleSize, 5> aItems;
	aItems.fill({170, 45});
	const auto Grid = ResolveSettingsToggleGrid(400, aItems, 10);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_GE(Grid.m_aColumnWidths[0], 170);
	const auto Narrow = ResolveSettingsToggleGrid(220, aItems, 10);
	EXPECT_EQ(Narrow.m_Columns, 1);
	EXPECT_GE(Narrow.m_aColumnWidths[0], 170);
}

TEST(SettingsToggleGrid, IncompleteLastRowFitsMeasuredAreaAndRetainsGaps)
{
	std::array<SSettingsToggleSize, 11> aItems;
	aItems.fill({130, 50});
	const auto Grid = ResolveSettingsToggleGrid(610, aItems, 10);
	const CUIRect Area{30, 50, 610, Grid.Height()};
	for(int Index = 0; Index < 11; ++Index)
	{
		const auto Cell = Grid.Cell(Area, Index);
		EXPECT_GE(Cell.x, Area.x);
		EXPECT_GE(Cell.y, Area.y);
		EXPECT_LE(Cell.x + Cell.w, Area.x + Area.w);
		EXPECT_LE(Cell.y + Cell.h, Area.y + Area.h);
		if(Index % Grid.m_Columns != 0)
		{
			const auto Previous = Grid.Cell(Area, Index - 1);
			EXPECT_FLOAT_EQ(Cell.x - Previous.x - Previous.w, Grid.m_Gap);
		}
	}
	EXPECT_FLOAT_EQ(Grid.Height(), 170);
}
