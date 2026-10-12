#include <game/client/QmUi/QmListEntryAnimation.h>

#include <gtest/gtest.h>

namespace
{
	constexpr auto INTERACTIVE = EQmListEntryRenderPass::INTERACTIVE;
	constexpr auto BACKGROUND = EQmListEntryRenderPass::BACKGROUND;
	constexpr auto MEASUREMENT = EQmListEntryRenderPass::MEASUREMENT;
}

TEST(QmListEntryAnimation, LongConsoleOverlayDoesNotReplayOnReturn)
{
	CQmListEntryAnimation Animation;
	EXPECT_LT(Animation.Resolve(1000, 1000, true, 2, INTERACTIVE), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1200, 1000, true, 2, INTERACTIVE), 0.0f);
	for(int64_t Now = 1216; Now <= 2400; Now += 16)
		EXPECT_FLOAT_EQ(Animation.Resolve(Now, 1000, true, 2, BACKGROUND), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(2416, 1000, true, 2, INTERACTIVE), 0.0f);
}

TEST(QmListEntryAnimation, ConsoleDuringEntryKeepsTheVisibleTrajectory)
{
	for(const int Level : {1, 2})
	{
		SCOPED_TRACE(Level);
		CQmListEntryAnimation Animation;
		CQmListEntryAnimation Reference;
		Animation.Resolve(1000, 1000, true, Level, INTERACTIVE);
		Reference.Resolve(1000, 1000, true, Level, INTERACTIVE);
		for(int64_t Now = 1016; Now < 1160; Now += 16)
		{
			const auto Pass = (Now - 1000) % 32 == 0 ? INTERACTIVE : BACKGROUND;
			EXPECT_FLOAT_EQ(Animation.Resolve(Now, 1000, true, Level, Pass), Reference.Resolve(Now, 1000, true, Level, INTERACTIVE));
		}
		EXPECT_FLOAT_EQ(Animation.Resolve(1200, 1000, true, Level, INTERACTIVE), 0.0f);
	}
}

TEST(QmListEntryAnimation, FirstVisibleFrameBehindConsoleStartsEntry)
{
	CQmListEntryAnimation Animation;
	EXPECT_FLOAT_EQ(Animation.Resolve(1000, 1000, true, 2, BACKGROUND), -12.0f);
	EXPECT_GT(Animation.Resolve(1080, 1000, true, 2, BACKGROUND), -12.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1200, 1000, true, 2, INTERACTIVE), 0.0f);
}

TEST(QmListEntryAnimation, ActualPageAbsenceStillReplaysEntry)
{
	CQmListEntryAnimation Animation;
	Animation.Resolve(1000, 1000, true, 2, INTERACTIVE);
	EXPECT_FLOAT_EQ(Animation.Resolve(1200, 1000, true, 2, INTERACTIVE), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1700, 1000, true, 2, INTERACTIVE), -12.0f);
}

TEST(QmListEntryAnimation, MeasurementDoesNotKeepAnAbsentPageAlive)
{
	CQmListEntryAnimation Animation;
	Animation.Resolve(1000, 1000, true, 2, INTERACTIVE);
	for(int64_t Now = 1016; Now <= 2400; Now += 16)
		EXPECT_FLOAT_EQ(Animation.Resolve(Now, 1000, true, 2, MEASUREMENT), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(2416, 1000, true, 2, INTERACTIVE), -12.0f);
}

TEST(QmListEntryAnimation, MeasurementDoesNotInitializeOrCancelVisibleEntry)
{
	CQmListEntryAnimation Animation;
	EXPECT_FLOAT_EQ(Animation.Resolve(1000, 1000, true, 2, MEASUREMENT), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1100, 1000, true, 2, INTERACTIVE), -12.0f);
	const float Before = Animation.Resolve(1140, 1000, true, 2, INTERACTIVE);
	EXPECT_FLOAT_EQ(Animation.Resolve(1140, 1000, false, 0, MEASUREMENT), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1140, 1000, true, 2, INTERACTIVE), Before);
}

TEST(QmListEntryAnimation, DisablingMotionBehindConsoleCancelsTheOldEntry)
{
	CQmListEntryAnimation Animation;
	Animation.Resolve(1000, 1000, true, 2, INTERACTIVE);
	EXPECT_FLOAT_EQ(Animation.Resolve(1040, 1000, false, 0, BACKGROUND), 0.0f);
	EXPECT_FLOAT_EQ(Animation.Resolve(1080, 1000, true, 2, INTERACTIVE), 0.0f);
}

TEST(QmListEntryAnimation, ResetStartsANewDisplayCycle)
{
	CQmListEntryAnimation Animation;
	Animation.Resolve(1000, 1000, true, 2, INTERACTIVE);
	Animation.Resolve(1200, 1000, true, 2, INTERACTIVE);
	Animation.Reset();
	EXPECT_FLOAT_EQ(Animation.Resolve(1216, 1000, true, 1, INTERACTIVE), -6.0f);
}
