#include <game/client/components/qmclient/demo_camera_path.h>

#include <gtest/gtest.h>

#include <limits>
#include <utility>

using namespace qm_demo_camera;

TEST(QmDemoCameraPath, EmptyTrackDoesNotTakeOverCamera)
{
	CTrack Track;
	EXPECT_FALSE(Track.Sample(100.0).has_value());
}

TEST(QmDemoCameraPath, KeyframesReachExactPositionAndZoomAndHoldAtBothEnds)
{
	CTrack Track;
	ASSERT_EQ(Track.Insert({100.0, {{10.0f, 20.0f}, 1.0f}}), 0);
	ASSERT_EQ(Track.Insert({200.0, {{110.0f, 220.0f}, 2.0f}}), 1);
	for(double Tick : {-1.0, 100.0})
	{
		const auto Sample = Track.Sample(Tick);
		ASSERT_TRUE(Sample.has_value());
		EXPECT_EQ(Sample->m_Center, vec2(10.0f, 20.0f));
		EXPECT_FLOAT_EQ(Sample->m_Zoom, 1.0f);
	}
	for(double Tick : {200.0, 250.0})
	{
		const auto Sample = Track.Sample(Tick);
		ASSERT_TRUE(Sample.has_value());
		EXPECT_EQ(Sample->m_Center, vec2(110.0f, 220.0f));
		EXPECT_FLOAT_EQ(Sample->m_Zoom, 2.0f);
	}
	const auto Sample = Track.Sample(150.0);
	ASSERT_TRUE(Sample.has_value());
	EXPECT_FLOAT_EQ(Sample->m_Zoom, 1.5f);
	EXPECT_EQ(Sample->m_Center, vec2(60.0f, 120.0f));
}

TEST(QmDemoCameraPath, SingleKeyframeKeepsOneCompositionThroughoutPlayback)
{
	CTrack Track;
	ASSERT_EQ(Track.Insert({100.0, {{10.0f, 20.0f}, 1.5f}}), 0);
	for(double Tick : {0.0, 100.0, 200.0})
	{
		const auto Sample = Track.Sample(Tick);
		ASSERT_TRUE(Sample.has_value());
		EXPECT_EQ(Sample->m_Center, vec2(10.0f, 20.0f));
		EXPECT_FLOAT_EQ(Sample->m_Zoom, 1.5f);
	}
}

TEST(QmDemoCameraPath, LinearAndHoldTransitionsHaveDifferentObservableBehavior)
{
	CTrack Track;
	ASSERT_EQ(Track.Insert({10.0, {{0.0f, 0.0f}, 1.0f}, ETransition::LINEAR}), 0);
	ASSERT_EQ(Track.Insert({20.0, {{100.0f, 0.0f}, 2.0f}}), 1);
	const auto Linear = Track.Sample(12.5);
	ASSERT_TRUE(Linear.has_value());
	EXPECT_FLOAT_EQ(Linear->m_Center.x, 25.0f);
	EXPECT_FLOAT_EQ(Linear->m_Zoom, 1.25f);
	ASSERT_EQ(Track.Insert({10.0, {{0.0f, 0.0f}, 1.0f}, ETransition::HOLD}), 0);
	const auto BeforeEnd = Track.Sample(19.99);
	const auto End = Track.Sample(20.0);
	ASSERT_TRUE(BeforeEnd.has_value());
	ASSERT_TRUE(End.has_value());
	EXPECT_FLOAT_EQ(BeforeEnd->m_Center.x, 0.0f);
	EXPECT_FLOAT_EQ(End->m_Center.x, 100.0f);
}

TEST(QmDemoCameraPath, SeekingBackwardAndRepeatingFramesDoesNotRetainMotionHistory)
{
	CTrack Track;
	ASSERT_GE(Track.Insert({100.0, {{0.0f, 0.0f}, 1.0f}}), 0);
	ASSERT_GE(Track.Insert({200.0, {{100.0f, 0.0f}, 2.0f}}), 0);
	const auto Before = Track.Sample(125.5);
	ASSERT_TRUE(Before.has_value());
	for(double Tick : {199.0, 101.0, 180.0, 150.0})
		ASSERT_TRUE(Track.Sample(Tick).has_value());
	const auto After = Track.Sample(125.5);
	ASSERT_TRUE(After.has_value());
	EXPECT_EQ(After->m_Center, Before->m_Center);
	EXPECT_FLOAT_EQ(After->m_Zoom, Before->m_Zoom);
	const auto Repeated = Track.Sample(125.5);
	ASSERT_TRUE(Repeated.has_value());
	EXPECT_EQ(Repeated->m_Center, Before->m_Center);
}

TEST(QmDemoCameraPath, RecordingAtAnExistingTimeUpdatesTheFrameWithoutAddingAnother)
{
	CTrack Track;
	ASSERT_EQ(Track.Insert({20.0, {{20.0f, 0.0f}, 1.0f}}), 0);
	ASSERT_EQ(Track.Insert({10.0, {{10.0f, 0.0f}, 1.0f}}), 0);
	ASSERT_EQ(Track.Insert({20.0, {{30.0f, 40.0f}, 2.0f}}), 1);
	ASSERT_EQ(Track.Frames().size(), 2U);
	const auto Updated = Track.Sample(20.0);
	ASSERT_TRUE(Updated.has_value());
	EXPECT_EQ(Updated->m_Center, vec2(30.0f, 40.0f));
	ASSERT_TRUE(Track.Erase(0));
	const auto Remaining = Track.Sample(10.0);
	ASSERT_TRUE(Remaining.has_value());
	EXPECT_EQ(Remaining->m_Center, vec2(30.0f, 40.0f));
	ASSERT_TRUE(Track.Erase(0));
	EXPECT_FALSE(Track.Sample(20.0).has_value());
	EXPECT_FALSE(Track.Erase(-1));
}

TEST(QmDemoCameraPath, InvalidInputAndCapacityFailurePreserveExistingFrames)
{
	CTrack Track;
	const SKeyframe Valid{10.0, {{10.0f, 20.0f}, 1.0f}};
	ASSERT_EQ(Track.Insert(Valid), 0);
	for(const SKeyframe &Invalid : {
		    SKeyframe{-1.0, Valid.m_View},
		    SKeyframe{20.0, {{std::numeric_limits<float>::infinity(), 0.0f}, 1.0f}},
		    SKeyframe{20.0, {{0.0f, 0.0f}, 0.0f}},
		    SKeyframe{20.0, Valid.m_View, ETransition::SMOOTH, 0.8, 0.2}})
		EXPECT_EQ(Track.Insert(Invalid), -1);
	EXPECT_EQ(Track.Frames().size(), 1U);
	EXPECT_FALSE(Track.Sample(std::numeric_limits<double>::quiet_NaN()).has_value());
	EXPECT_FALSE(Track.SetAspect(0.0f));
	for(size_t i = 0; i < CTrack::MAX_KEYFRAMES; ++i)
		ASSERT_GE(Track.Insert({static_cast<double>(i), Valid.m_View}), 0);
	EXPECT_EQ(Track.Insert({10000.0, Valid.m_View}), -1);
	EXPECT_GE(Track.Insert({10.0, {{5.0f, 6.0f}, 2.0f}}), 0);
	EXPECT_EQ(Track.Frames().size(), CTrack::MAX_KEYFRAMES);
}

TEST(QmDemoCameraPath, SparseDemoTicksUseTheActualFractionalPlaybackPosition)
{
	EXPECT_DOUBLE_EQ(PlaybackTick(100, 110, 0.25f), 102.5);
	EXPECT_DOUBLE_EQ(PlaybackTick(100, 110, -1.0f), 100.0);
	EXPECT_DOUBLE_EQ(PlaybackTick(100, 110, 2.0f), 110.0);
	EXPECT_DOUBLE_EQ(PlaybackTick(100, 100, 0.5f), 100.0);
}

TEST(QmDemoCameraPath, CuttingInsideASmoothMovePreservesTheOriginalSpeedAndComposition)
{
	CTrack Track;
	ASSERT_GE(Track.Insert({100.0, {{0.0f, 0.0f}, 1.0f}}), 0);
	ASSERT_GE(Track.Insert({200.0, {{100.0f, 50.0f}, 2.0f}}), 0);
	const auto Cut = Track.Cut({{125, 175}});
	ASSERT_TRUE(Cut.has_value());
	for(double Tick : {125.0, 126.0, 140.5, 150.0, 174.5, 175.0})
	{
		SCOPED_TRACE(Tick);
		const auto CutSample = Cut->Sample(Tick);
		const auto OriginalSample = Track.Sample(Tick);
		ASSERT_TRUE(CutSample.has_value());
		ASSERT_TRUE(OriginalSample.has_value());
		EXPECT_NEAR(CutSample->m_Center.x, OriginalSample->m_Center.x, 0.0001f);
		EXPECT_NEAR(CutSample->m_Zoom, OriginalSample->m_Zoom, 0.00001f);
	}
	const auto Recut = Cut->Cut({{130, 160}});
	ASSERT_TRUE(Recut.has_value());
	const auto RecutSample = Recut->Sample(145.0);
	const auto OriginalSample = Track.Sample(145.0);
	ASSERT_TRUE(RecutSample.has_value());
	ASSERT_TRUE(OriginalSample.has_value());
	EXPECT_NEAR(RecutSample->m_Center.x, OriginalSample->m_Center.x, 0.0001f);
}

TEST(QmDemoCameraPath, MultipleCutsUseDemoEditorTickOffsetsAndDoNotBridgeMissingFootage)
{
	CTrack Track;
	ASSERT_GE(Track.Insert({100.0, {{0.0f, 0.0f}, 1.0f}, ETransition::LINEAR}), 0);
	ASSERT_GE(Track.Insert({300.0, {{200.0f, 0.0f}, 3.0f}}), 0);
	const auto Cut = Track.Cut({{250, 275}, {125, 150}});
	ASSERT_TRUE(Cut.has_value());
	for(const auto &Ticks : {std::pair{125.0, 125.0}, {150.9, 150.0}, {151.0, 250.0}, {163.5, 262.5}, {176.0, 275.0}})
	{
		SCOPED_TRACE(Ticks.first);
		const auto CutSample = Cut->Sample(Ticks.first);
		const auto OriginalSample = Track.Sample(Ticks.second);
		ASSERT_TRUE(CutSample.has_value());
		ASSERT_TRUE(OriginalSample.has_value());
		EXPECT_EQ(CutSample->m_Center, OriginalSample->m_Center);
	}
	EXPECT_FALSE(Track.Cut({{200, 100}}).has_value());
}
