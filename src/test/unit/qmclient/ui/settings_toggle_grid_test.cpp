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

TEST(SettingsToggleGrid, FiveTogglesUseThreeAndTwoInsteadOfLeavingOneOnTheLastRow)
{
	const std::array<SSettingsToggleSize, 5> aItems = {{{220, 30}, {90, 50}, {170, 80}, {150, 100}, {220, 40}}};
	const auto Grid = ResolveSettingsToggleGrid(660, aItems, 10);
	const CUIRect Area{20, 40, 660, Grid.Height()};
	EXPECT_EQ(Grid.m_Columns, 3);
	EXPECT_EQ(Grid.m_Rows, 2);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 2).y, Area.y);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 3).y, Grid.Cell(Area, 4).y);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 3).x, Area.x);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 4).x - Grid.Cell(Area, 3).x - Grid.Cell(Area, 3).w, 10);
	EXPECT_FLOAT_EQ(Grid.Height(), 190);
	for(int Index = 0; Index < static_cast<int>(aItems.size()); ++Index)
	{
		const auto Cell = Grid.Cell(Area, Index);
		EXPECT_GE(Cell.w, aItems[Index].m_Width);
		EXPECT_GE(Cell.h, aItems[Index].m_Height);
		EXPECT_LE(Cell.x + Cell.w, Area.x + Area.w);
		EXPECT_LE(Cell.y + Cell.h, Area.y + Area.h);
	}
}

TEST(SettingsToggleGrid, FourDynamicTogglesUseTwoPerRowAndReflowWhenResized)
{
	const std::vector<SSettingsToggleSize> vItems = {{240, 30}, {200, 30}, {200, 30}, {240, 30}};
	SSettingsDynamicToggleGrid Grid;
	ResolveSettingsToggleGrid(660, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_EQ(Grid.m_Rows, 2);
	const CUIRect Area{20, 40, 660, Grid.Height()};
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 0).y, Grid.Cell(Area, 1).y);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 2).y, Grid.Cell(Area, 3).y);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 3).x + Grid.Cell(Area, 3).w, Area.x + Area.w);

	ResolveSettingsToggleGrid(910, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 4);
	EXPECT_EQ(Grid.m_Rows, 1);
	ResolveSettingsToggleGrid(660, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_EQ(Grid.m_Rows, 2);
	ResolveSettingsToggleGrid(250, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 1);
	EXPECT_EQ(Grid.m_Rows, 4);
}

TEST(SettingsToggleGrid, BalancedColumnsMustStillFitTheReassignedLabels)
{
	const std::array<SSettingsToggleSize, 5> aItems = {{{300, 30}, {50, 30}, {50, 30}, {50, 30}, {300, 30}}};
	const auto Grid = ResolveSettingsToggleGrid(480, aItems, 10);
	const CUIRect Area{20, 40, 480, Grid.Height()};
	EXPECT_EQ(Grid.m_Columns, 4);
	EXPECT_EQ(Grid.m_Rows, 2);
	for(int Index = 0; Index < static_cast<int>(aItems.size()); ++Index)
	{
		const auto Cell = Grid.Cell(Area, Index);
		EXPECT_GE(Cell.w, aItems[Index].m_Width);
		EXPECT_LE(Cell.x + Cell.w, Area.x + Area.w);
	}
}

TEST(SettingsToggleGrid, BalancingDoesNotAddRowsToANarrowGroup)
{
	std::array<SSettingsToggleSize, 7> aItems;
	aItems.fill({100, 30});
	const auto Grid = ResolveSettingsToggleGrid(320, aItems, 10);
	EXPECT_EQ(Grid.m_Columns, 3);
	EXPECT_EQ(Grid.m_Rows, 3);
	EXPECT_FLOAT_EQ(Grid.Height(), 110);
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

TEST(SettingsToggleGrid, ShortLabelFitsOneTextButtonWithoutASeparateControlRow)
{
	const auto Size = ResolveSettingsToggleButtonSize(100, 12, 20, 8, 3);
	const CUIRect Button{30, 50, Size.m_Width, Size.m_Height};
	const CUIRect Label = ResolveSettingsToggleButtonLabel(Button, 8, 3);
	EXPECT_FLOAT_EQ(Button.w, 116);
	EXPECT_FLOAT_EQ(Button.h, 20);
	EXPECT_FLOAT_EQ(Label.w, 100);
	EXPECT_GE(Label.h, 12);
	EXPECT_FLOAT_EQ(Label.x + Label.w * 0.5f, Button.x + Button.w * 0.5f);
	EXPECT_FLOAT_EQ(Label.y + Label.h * 0.5f, Button.y + Button.h * 0.5f);
}

TEST(SettingsToggleGrid, WrappedButtonLabelExpandsItsRowAndKeepsThePadding)
{
	std::array<SSettingsToggleSize, 2> aItems = {{
		ResolveSettingsToggleButtonSize(100, 12, 20, 8, 3),
		ResolveSettingsToggleButtonSize(100, 40, 20, 8, 3)}};
	const auto Grid = ResolveSettingsToggleGrid(240, aItems, 8);
	const CUIRect Area{10, 20, 240, Grid.Height()};
	const CUIRect Label = ResolveSettingsToggleButtonLabel(Grid.Cell(Area, 1), 8, 3);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_FLOAT_EQ(Grid.Height(), 46);
	EXPECT_FLOAT_EQ(Label.h, 40);
	EXPECT_FLOAT_EQ(Label.y + Label.h + 3, Area.y + Area.h);
}

TEST(SettingsToggleGrid, TinyTextButtonKeepsItsLabelInsideWithoutNegativeDimensions)
{
	for(const float Width : {0.0f, 5.0f, 12.0f})
	{
		SCOPED_TRACE(Width);
		const CUIRect Button{10, 20, Width, 4};
		const CUIRect Label = ResolveSettingsToggleButtonLabel(Button, 8, 3);
		EXPECT_GE(Label.x, Button.x);
		EXPECT_GE(Label.y, Button.y);
		EXPECT_GE(Label.w, 0);
		EXPECT_GE(Label.h, 0);
		EXPECT_LE(Label.x + Label.w, Button.x + Button.w);
		EXPECT_LE(Label.y + Label.h, Button.y + Button.h);
	}
}

TEST(SettingsToggleGrid, ScalingTextButtonPreservesTheLabelInsetAndMeasuredHeight)
{
	const auto Base = ResolveSettingsToggleButtonSize(100, 40, 20, 8, 3);
	for(const float Scale : {0.5f, 1.25f, 2.0f})
	{
		SCOPED_TRACE(Scale);
		const auto Size = ResolveSettingsToggleButtonSize(100 * Scale, 40 * Scale, 20 * Scale, 8 * Scale, 3 * Scale);
		const CUIRect Label = ResolveSettingsToggleButtonLabel({0, 0, Size.m_Width, Size.m_Height}, 8 * Scale, 3 * Scale);
		EXPECT_FLOAT_EQ(Size.m_Width, Base.m_Width * Scale);
		EXPECT_FLOAT_EQ(Size.m_Height, Base.m_Height * Scale);
		EXPECT_FLOAT_EQ(Label.w, 100 * Scale);
		EXPECT_FLOAT_EQ(Label.h, 40 * Scale);
	}
}

TEST(SettingsToggleGrid, BooleanClicksToggleOnlyTheSelectedConfiguration)
{
	int First = 0;
	int Second = 1;
	const SSettingsToggleEntry Entry{&First, "first", "First"};
	EXPECT_TRUE(ApplySettingsToggleEntry(Entry, true, true, false));
	EXPECT_EQ(First, 1);
	EXPECT_EQ(Second, 1);
	EXPECT_TRUE(ApplySettingsToggleEntry(Entry, true, true, false));
	EXPECT_EQ(First, 0);
	EXPECT_FALSE(ApplySettingsToggleEntry(Entry, false, true, false));
	EXPECT_EQ(First, 0);
}

TEST(SettingsToggleGrid, ReadOnlyAndTemporarilyOverriddenEntriesRejectClicks)
{
	int Value = 1;
	const SSettingsToggleEntry Entry{&Value, "value", "Value"};
	EXPECT_FALSE(ApplySettingsToggleEntry(Entry, true, false, false));
	EXPECT_FALSE(ApplySettingsToggleEntry(Entry, true, true, true));
	EXPECT_EQ(Value, 1);
	EXPECT_TRUE(ApplySettingsToggleEntry(Entry, true, true, false));
	EXPECT_EQ(Value, 0);
}

TEST(SettingsToggleGrid, BitmaskEntriesKeepOtherEffectsAndSupportRepeatedClicks)
{
	int Effects = 1 | 4;
	const SSettingsToggleEntry Entry{&Effects, "gradient", "Gradient", "gradient", 2};
	EXPECT_FALSE(SettingsToggleEntryValue(Entry));
	EXPECT_TRUE(ApplySettingsToggleEntry(Entry, true, true, false));
	EXPECT_EQ(Effects, 1 | 2 | 4);
	EXPECT_TRUE(SettingsToggleEntryValue(Entry));
	EXPECT_TRUE(ApplySettingsToggleEntry(Entry, true, true, false));
	EXPECT_EQ(Effects, 1 | 4);
}

TEST(SettingsToggleGrid, DynamicGroupReflowsUsingAvailableWidth)
{
	const std::vector<SSettingsToggleSize> vItems(11, {100, 45});
	SSettingsDynamicToggleGrid Grid;
	ResolveSettingsToggleGrid(700, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 6);
	EXPECT_EQ(Grid.m_Rows, 2);
	ResolveSettingsToggleGrid(340, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 3);
	EXPECT_EQ(Grid.m_Rows, 4);
	ResolveSettingsToggleGrid(700, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 6);
	EXPECT_EQ(Grid.m_Rows, 2);
}

TEST(SettingsToggleGrid, DynamicGroupUsesColumnMaximaAndRemeasuredRowHeights)
{
	std::vector<SSettingsToggleSize> vItems = {{240, 30}, {60, 50}, {220, 70}, {50, 20}};
	SSettingsDynamicToggleGrid Grid;
	ResolveSettingsToggleGrid(350, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 2);
	EXPECT_FLOAT_EQ(Grid.m_aColumnWidths[0], 260);
	EXPECT_FLOAT_EQ(Grid.m_aColumnWidths[1], 80);
	EXPECT_FLOAT_EQ(Grid.Height(), 130);
	vItems[0].m_Height = 80;
	Grid.SetItemHeights(vItems);
	const CUIRect Area{20, 40, 350, Grid.Height()};
	EXPECT_FLOAT_EQ(Grid.Height(), 160);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 2).y, 130);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 3).y + Grid.Cell(Area, 3).h, Area.y + Area.h);
}

TEST(SettingsToggleGrid, ReusedDynamicGroupDropsPreviousWidthsHeightsAndEmptyRows)
{
	std::vector<SSettingsToggleSize> vItems(6, {300, 80});
	SSettingsDynamicToggleGrid Grid;
	ResolveSettingsToggleGrid(350, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 1);
	EXPECT_FLOAT_EQ(Grid.Height(), 530);
	vItems.assign(3, {50, 20});
	ResolveSettingsToggleGrid(350, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Columns, 3);
	EXPECT_EQ(Grid.m_Rows, 1);
	EXPECT_FLOAT_EQ(Grid.Height(), 20);
	vItems.clear();
	ResolveSettingsToggleGrid(350, vItems, 10, Grid);
	EXPECT_EQ(Grid.m_Rows, 0);
	EXPECT_FLOAT_EQ(Grid.Height(), 0);
	vItems.assign(2, {50, 20});
	ResolveSettingsToggleGrid(5, vItems, 10, Grid);
	const CUIRect Area{10, 20, 5, Grid.Height()};
	EXPECT_EQ(Grid.m_Columns, 1);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 1).w, 5);
	EXPECT_FLOAT_EQ(Grid.Cell(Area, 1).y + Grid.Cell(Area, 1).h, Area.y + Area.h);
}
