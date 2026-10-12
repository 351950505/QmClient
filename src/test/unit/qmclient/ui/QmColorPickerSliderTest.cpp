// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/QmUi/ColorPickerSlider.h>

#include <gtest/gtest.h>
#include <test/test.h>

// 取色器通道滑条几何：纯换算，无 UI 依赖。
// 回归点：轨道横向内缩而圆心行程仍用整段宽度，会让 0 / 255 端的滑块悬在轨道端外。

namespace
{
	// 取色器弹窗单行滑条的实际宽度量级（PICKER_POPUP_WIDTH 扣除标签与数值框后约 216）。
	constexpr float TEST_TRACK_WIDTH = 216.0f;
	constexpr int TEST_MAX_VALUE = 255;
} // namespace

TEST(QmColorPickerSlider, KnobStaysInsideTrackAtBothEnds)
{
	const float LeftCenter = qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 0.0f);
	const float RightCenter = qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 1.0f);
	const float Radius = qm_picker_slider::KNOB_RADIUS;

	// 最小值时滑块左沿贴住轨道左沿，最大值时右沿贴住轨道右沿，两端都不外溢。
	EXPECT_FLOAT_EQ(LeftCenter - Radius, 0.0f);
	EXPECT_FLOAT_EQ(RightCenter + Radius, TEST_TRACK_WIDTH);
	EXPECT_GE(LeftCenter - Radius, 0.0f);
	EXPECT_LE(RightCenter + Radius, TEST_TRACK_WIDTH);
}

TEST(QmColorPickerSlider, KnobCenterGrowsMonotonicallyWithValue)
{
	float Previous = qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 0.0f);
	for(int Value = 1; Value <= TEST_MAX_VALUE; ++Value)
	{
		const float Current = qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, Value / (float)TEST_MAX_VALUE);
		EXPECT_GT(Current, Previous);
		Previous = Current;
	}
	// 越界输入夹紧在轨道内。
	EXPECT_FLOAT_EQ(qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, -1.0f), qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 0.0f));
	EXPECT_FLOAT_EQ(qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 2.0f), qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, 1.0f));
}

TEST(QmColorPickerSlider, TrackOffsetRoundTripsWithKnobCenter)
{
	// 点击轨道上滑块圆心所在位置，必须换算回同一个值，避免拖动与落点分离。
	for(int Value = 0; Value <= TEST_MAX_VALUE; ++Value)
	{
		const float Normalized = Value / (float)TEST_MAX_VALUE;
		const float Offset = qm_picker_slider::KnobCenterOffset(TEST_TRACK_WIDTH, Normalized);
		EXPECT_NEAR(qm_picker_slider::ValueFromTrackOffset(TEST_TRACK_WIDTH, Offset), Normalized, 1e-4f);
	}
}

TEST(QmColorPickerSlider, TrackEndsAndBeyondClampToExtremes)
{
	// 轨道两端各一个半径的区间没有对应的滑块圆心，统一归到端点值。
	EXPECT_FLOAT_EQ(qm_picker_slider::ValueFromTrackOffset(TEST_TRACK_WIDTH, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(qm_picker_slider::ValueFromTrackOffset(TEST_TRACK_WIDTH, qm_picker_slider::KNOB_RADIUS * 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(qm_picker_slider::ValueFromTrackOffset(TEST_TRACK_WIDTH, TEST_TRACK_WIDTH), 1.0f);
	EXPECT_FLOAT_EQ(qm_picker_slider::ValueFromTrackOffset(TEST_TRACK_WIDTH, TEST_TRACK_WIDTH - qm_picker_slider::KNOB_RADIUS * 0.5f), 1.0f);
}

TEST(QmColorPickerSlider, DegenerateTrackWidthKeepsKnobInsideAndFinite)
{
	// 窄到放不下两个半径时行程为 0，滑块停在左端内缩处，换算退化为 0 且不产生 NaN。
	const float NarrowWidth = qm_picker_slider::KNOB_RADIUS;
	EXPECT_FLOAT_EQ(qm_picker_slider::KnobTravel(NarrowWidth), 0.0f);
	EXPECT_FLOAT_EQ(qm_picker_slider::KnobCenterOffset(NarrowWidth, 1.0f), qm_picker_slider::KNOB_RADIUS);
	EXPECT_FLOAT_EQ(qm_picker_slider::ValueFromTrackOffset(NarrowWidth, NarrowWidth * 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(qm_picker_slider::KnobTravel(0.0f), 0.0f);
}
