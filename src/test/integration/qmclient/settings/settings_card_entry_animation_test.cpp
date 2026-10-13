// 设置卡片入场调度与真实动画后端协作，覆盖生产动效策略和分类切换。
#include <engine/shared/config.h>

#include <game/client/QmUi/SettingsCardDeckLogic.h>
#include <game/client/QmUi/SettingsCardGeometry.h>

#include <gtest/gtest.h>

namespace
{
	struct SEntryRuntime
	{
		CSettingsCardDeckFrameRuntime m_Frame;
		CUiV2AnimationRuntime m_Animation;
		SCardMotionSpec m_Motion;
		const char *m_pTab = "a";

		float Resolve()
		{
			return m_Frame.ResolveEntryOffset(m_Animation, 100, m_pTab, m_Motion);
		}

		float Enter(uint64_t Cycle, const char *pTab, bool AnimateEntry = true)
		{
			m_Frame.BeginDisplayCycle(Cycle, AnimateEntry);
			m_Frame.OnTabChanged(m_Motion.m_ContinuousEntry);
			m_pTab = pTab;
			return Resolve();
		}

		void Settle()
		{
			for(int Frame = 0; Frame < 120 && m_Frame.EntryWasActive(); ++Frame)
			{
				m_Animation.Advance(1.0f / 120.0f);
				Resolve();
			}
		}
	};

	class SettingsCardEntryAnimation : public ::testing::TestWithParam<int>
	{
		int m_PreviousMotionLevel = 0;

	protected:
		void SetUp() override
		{
			m_PreviousMotionLevel = g_Config.m_QmUiMotionLevel;
			g_Config.m_QmUiMotionLevel = GetParam();
		}

		void TearDown() override
		{
			g_Config.m_QmUiMotionLevel = m_PreviousMotionLevel;
		}

		SCardMotionSpec Motion(bool ListEntryAnimations = true) const
		{
			return ResolveCardMotionSpec(GetParam(), ListEntryAnimations, true, true, true);
		}
	};
}

TEST_P(SettingsCardEntryAnimation, ConsecutiveAndReversePageSwitchesKeepPositionAndMotion)
{
	SEntryRuntime Switched;
	SEntryRuntime Reference;
	Switched.m_Motion = Reference.m_Motion = Motion();
	const float Distance = Switched.m_Motion.m_EntryDistance;
	EXPECT_FLOAT_EQ(Switched.Enter(1, "a"), Distance);
	EXPECT_FLOAT_EQ(Reference.Enter(1, "a"), Distance);
	const float InitialDt = Switched.m_Motion.m_EntryDuration * 0.25f;
	Switched.m_Animation.Advance(InitialDt);
	Reference.m_Animation.Advance(InitialDt);
	const float BeforeSwitch = Switched.Resolve();
	ASSERT_GT(BeforeSwitch, 0.0f);
	ASSERT_LT(BeforeSwitch, Distance);
	EXPECT_NEAR(BeforeSwitch, Reference.Resolve(), 0.0001f);

	EXPECT_NEAR(Switched.Enter(2, "b"), BeforeSwitch, 0.0001f);
	const float NextDt = Switched.m_Motion.m_EntryDuration * 0.05f;
	Switched.m_Animation.Advance(NextDt);
	Reference.m_Animation.Advance(NextDt);
	const float AfterSwitch = Switched.Resolve();
	EXPECT_NEAR(AfterSwitch, Reference.Resolve(), 0.0001f);
	EXPECT_LT(AfterSwitch, BeforeSwitch);

	EXPECT_NEAR(Switched.Enter(3, "a"), AfterSwitch, 0.0001f);
	Switched.m_Animation.Advance(NextDt);
	Reference.m_Animation.Advance(NextDt);
	EXPECT_NEAR(Switched.Resolve(), Reference.Resolve(), 0.0001f);
	EXPECT_EQ(Switched.m_Animation.ActiveTrackCount(), 1);
	EXPECT_EQ(Switched.m_Animation.QueuedTrackCount(), 0);
}

TEST_P(SettingsCardEntryAnimation, SettledEntryStartsAgainForNextPage)
{
	SEntryRuntime Runtime;
	Runtime.m_Motion = Motion();
	Runtime.Enter(1, "a");
	Runtime.Settle();
	ASSERT_FALSE(Runtime.m_Frame.EntryWasActive());
	ASSERT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);
	EXPECT_FLOAT_EQ(Runtime.Resolve(), 0.0f);

	EXPECT_FLOAT_EQ(Runtime.Enter(2, "b"), Runtime.m_Motion.m_EntryDistance);
	EXPECT_TRUE(Runtime.m_Frame.EntryWasActive());
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 1);
}

TEST_P(SettingsCardEntryAnimation, MotionLevelZeroCancelsSharedEntryAndRestoreStartsFresh)
{
	SEntryRuntime Runtime;
	Runtime.m_Motion = Motion();
	Runtime.Enter(1, "a");
	Runtime.m_Animation.Advance(Runtime.m_Motion.m_EntryDuration * 0.25f);
	ASSERT_GT(Runtime.Resolve(), 0.0f);

	g_Config.m_QmUiMotionLevel = 0;
	Runtime.m_Motion = ResolveCardMotionSpec(0, true, true, true, true);
	EXPECT_FLOAT_EQ(Runtime.Resolve(), 0.0f);
	EXPECT_FALSE(Runtime.m_Frame.EntryWasActive());
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);
	EXPECT_EQ(Runtime.m_Animation.QueuedTrackCount(), 0);

	g_Config.m_QmUiMotionLevel = GetParam();
	Runtime.m_Motion = Motion();
	EXPECT_FLOAT_EQ(Runtime.Resolve(), 0.0f);
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);
	EXPECT_FLOAT_EQ(Runtime.Enter(2, "b"), Runtime.m_Motion.m_EntryDistance);
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 1);
}

TEST_P(SettingsCardEntryAnimation, DisabledListEntryCancelsSharedTrackAndSkipsPageChanges)
{
	SEntryRuntime Runtime;
	Runtime.m_Motion = Motion();
	Runtime.Enter(1, "a");
	Runtime.m_Animation.Advance(Runtime.m_Motion.m_EntryDuration * 0.25f);
	ASSERT_GT(Runtime.Resolve(), 0.0f);

	Runtime.m_Motion = Motion(false);
	EXPECT_FLOAT_EQ(Runtime.Resolve(), 0.0f);
	EXPECT_FALSE(Runtime.m_Frame.EntryWasActive());
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);
	EXPECT_FLOAT_EQ(Runtime.Enter(2, "b"), 0.0f);
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);

	Runtime.m_Motion = Motion();
	EXPECT_FLOAT_EQ(Runtime.Resolve(), 0.0f);
	EXPECT_FLOAT_EQ(Runtime.Enter(3, "a"), Runtime.m_Motion.m_EntryDistance);
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 1);
}

TEST_P(SettingsCardEntryAnimation, NonAnimatedDisplayCycleCancelsSharedTrack)
{
	SEntryRuntime Runtime;
	Runtime.m_Motion = Motion();
	Runtime.Enter(1, "a");
	Runtime.m_Animation.Advance(Runtime.m_Motion.m_EntryDuration * 0.25f);
	ASSERT_GT(Runtime.Resolve(), 0.0f);

	EXPECT_FLOAT_EQ(Runtime.Enter(2, "b", false), 0.0f);
	EXPECT_FALSE(Runtime.m_Frame.EntryWasActive());
	EXPECT_EQ(Runtime.m_Animation.ActiveTrackCount(), 0);
	EXPECT_FLOAT_EQ(Runtime.Enter(3, "a"), Runtime.m_Motion.m_EntryDistance);
}

TEST_P(SettingsCardEntryAnimation, CustomMotionRetainsSeparateTabEntry)
{
	SEntryRuntime Runtime;
	Runtime.m_Motion.m_EntryDistance = 8.0f;
	Runtime.m_Motion.m_EntryDuration = 0.2f;
	EXPECT_FLOAT_EQ(Runtime.Enter(1, "a"), 8.0f);
	Runtime.m_Animation.Advance(0.025f);
	const float BeforeSwitch = Runtime.Resolve();
	ASSERT_GT(BeforeSwitch, 0.0f);
	ASSERT_LT(BeforeSwitch, 8.0f);

	EXPECT_FLOAT_EQ(Runtime.Enter(2, "b"), 8.0f);
	EXPECT_TRUE(Runtime.m_Frame.EntryWasActive());
}

INSTANTIATE_TEST_SUITE_P(MotionLevels, SettingsCardEntryAnimation, ::testing::Values(1, 2));
