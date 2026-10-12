#include <engine/shared/config.h>

#include <game/client/QmUi/QmScroll.h>

#include <gtest/gtest.h>

TEST(QmScrollMode, OriginalModeAppliesTheSameWheelStepImmediately)
{
	CQmScrollState State;
	const SQmScrollConfig Config = QmScrollConfigForMode(QmSettingsScrollConfig(1.0f, 0.5f), 0);
	State.AddWheelImpulse(-120.0f, {200.0f, 1000.0f}, Config);
	EXPECT_FLOAT_EQ(State.Offset(), 120.0f);
	EXPECT_FLOAT_EQ(State.Velocity(), 0.0f);
	EXPECT_FALSE(State.Animating());
}

TEST(QmScrollMode, DampedModeKeepsTheRequestedAnimation)
{
	CQmScrollState State;
	const SQmScrollConfig Config = QmScrollConfigForMode(QmSettingsScrollConfig(1.0f, 0.5f), 1);
	State.AddWheelImpulse(-120.0f, {200.0f, 1000.0f}, Config);
	EXPECT_TRUE(State.Animating());
	EXPECT_FLOAT_EQ(State.Offset(), 0.0f);
	State.Advance(1.0f / 60.0f, {200.0f, 1000.0f}, Config);
	EXPECT_GT(State.Offset(), 0.0f);
	EXPECT_LT(State.Offset(), 120.0f);
}

TEST(QmScrollMode, SwitchingToOriginalFinishesPendingMotionAndCanResumeDamping)
{
	CQmScrollState State;
	const SQmScrollMetrics Metrics{200.0f, 1000.0f};
	const auto Damped = QmSettingsScrollConfig(1.0f, 0.5f);
	State.AddWheelImpulse(-120.0f, Metrics, Damped);
	State.Advance(1.0f / 60.0f, Metrics, Damped);
	State.Advance(0.0f, Metrics, QmScrollConfigForMode(Damped, 0));
	EXPECT_FLOAT_EQ(State.Offset(), 120.0f);
	EXPECT_FLOAT_EQ(State.Velocity(), 0.0f);
	EXPECT_FALSE(State.Animating());
	State.AddWheelImpulse(-120.0f, Metrics, Damped);
	EXPECT_TRUE(State.Animating());
	State.Advance(1.0f / 60.0f, Metrics, Damped);
	EXPECT_GT(State.Offset(), 120.0f);
	EXPECT_LT(State.Offset(), 240.0f);
}

namespace
{
	class QmConfiguredScrollMode : public testing::Test
	{
	protected:
		CConfig m_Previous = g_Config;
		void TearDown() override { g_Config = m_Previous; }
	};
}

TEST_F(QmConfiguredScrollMode, SharedControllerKeepsAltAccelerationInOriginalMode)
{
	g_Config.m_QmUiScrollMode = 0;
	CQmScrollState State;
	CQmScrollController Controller;
	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_WheelDelta = -120.0f;
	Input.m_AltPressed = true;
	Controller.Update(State, {0.0f, 0.0f, 300.0f, 200.0f}, 2000.0f, 1.0f / 60.0f, Input, {}, QmSettingsScrollConfig(1.0f, 0.5f));
	EXPECT_FLOAT_EQ(State.Offset(), 120.0f * QmScrollAltMultiplier());
	EXPECT_FALSE(State.Animating());
}
