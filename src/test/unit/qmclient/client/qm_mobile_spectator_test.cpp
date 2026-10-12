#include <game/client/components/qmclient/mobile_spectator_controls.h>

#include <gtest/gtest.h>

#include <limits>

using namespace qm_mobile_spectator;

TEST(QmMobileSpectator, JoystickCenterAndDeadzoneDoNotMove)
{
	EXPECT_EQ(Direction(vec2(0.0f, 0.0f), 50.0f), vec2(0.0f, 0.0f));
	EXPECT_EQ(Direction(vec2(5.0f, 0.0f), 50.0f), vec2(0.0f, 0.0f));
}

TEST(QmMobileSpectator, JoystickClampsDiagonalAndOutsideDrags)
{
	EXPECT_NEAR(length(Direction(vec2(100.0f, 100.0f), 50.0f)), 1.0f, 0.00001f);
	EXPECT_EQ(Direction(vec2(-200.0f, 0.0f), 50.0f), vec2(-1.0f, 0.0f));
}

TEST(QmMobileSpectator, CameraMovesAtSameSpeedAcrossFrameRates)
{
	vec2 At30(0.0f, 0.0f), At60(0.0f, 0.0f);
	for(int i = 0; i < 30; ++i)
		At30 += Movement(vec2(1.0f, 0.0f), 1.0f, 1.0f / 30.0f);
	for(int i = 0; i < 60; ++i)
		At60 += Movement(vec2(1.0f, 0.0f), 1.0f, 1.0f / 60.0f);
	EXPECT_NEAR(At30.x, 1000.0f, 0.001f);
	EXPECT_NEAR(At30.x, At60.x, 0.001f);
	EXPECT_EQ(Movement(vec2(1.0f, 0.0f), 2.0f, 0.01f), vec2(20.0f, 0.0f));
}

TEST(QmMobileSpectator, InvalidFrameTimeStopsAndLongFrameIsBounded)
{
	EXPECT_EQ(Movement(vec2(1.0f, 0.0f), 1.0f, -1.0f), vec2(0.0f, 0.0f));
	EXPECT_EQ(Movement(vec2(1.0f, 0.0f), 1.0f, std::numeric_limits<float>::quiet_NaN()), vec2(0.0f, 0.0f));
	EXPECT_EQ(Movement(vec2(1.0f, 0.0f), 1.0f, 10.0f), vec2(50.0f, 0.0f));
}

namespace
{
	const vec2 Screen(720.0f, 400.0f);
	std::vector<IInput::CTouchFingerState> FingerAt(vec2 Position)
	{
		IInput::CTouchFingerState Finger{};
		Finger.m_Position = Position / Screen;
		return {Finger};
	}
}

TEST(QmMobileSpectator, HeldJoystickMovesWithoutFurtherFingerDelta)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	const auto Fingers = FingerAt(Geometry.m_Joystick.Center() + vec2(40.0f, 0.0f));
	Touch.Update(Fingers, Geometry, Screen);
	const vec2 FirstDirection = Touch.JoystickDirection();
	Touch.Update(Fingers, Geometry, Screen);
	EXPECT_GT(FirstDirection.x, 0.0f);
	EXPECT_EQ(Touch.JoystickDirection(), FirstDirection);
}

TEST(QmMobileSpectator, ReleaseStopsJoystickImmediately)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	Touch.Update(FingerAt(Geometry.m_Joystick.Center() + vec2(40.0f, 0.0f)), Geometry, Screen);
	Touch.Update({}, Geometry, Screen);
	EXPECT_EQ(Touch.JoystickDirection(), vec2(0.0f, 0.0f));
}

TEST(QmMobileSpectator, CancelRequiresReleaseBeforeJoystickCanRestart)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	const auto Fingers = FingerAt(Geometry.m_Joystick.Center() + vec2(40.0f, 0.0f));
	Touch.Update(Fingers, Geometry, Screen);
	Touch.Cancel();
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 0U);
	EXPECT_EQ(Touch.JoystickDirection(), vec2(0.0f, 0.0f));
	Touch.Update({}, Geometry, Screen);
	Touch.Update(Fingers, Geometry, Screen);
	EXPECT_GT(Touch.JoystickDirection().x, 0.0f);
}

TEST(QmMobileSpectator, HeldButtonFiresOnceAndCanBePressedAgain)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	const auto Fingers = FingerAt(Geometry.m_aButtons[ZOOM_IN].Center());
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 1U << ZOOM_IN);
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 0U);
	Touch.Update({}, Geometry, Screen);
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 1U << ZOOM_IN);
}

TEST(QmMobileSpectator, JoystickDragAcrossButtonDoesNotActivateButton)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	Touch.Update(FingerAt(Geometry.m_Joystick.Center()), Geometry, Screen);
	EXPECT_EQ(Touch.Update(FingerAt(Geometry.m_aButtons[MENU].Center()), Geometry, Screen), 0U);
}

TEST(QmMobileSpectator, PlayerSelectorOpensAfterOpeningFingerIsReleased)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	const auto Fingers = FingerAt(Geometry.m_aButtons[SELECT].Center());
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 0U);
	EXPECT_EQ(Touch.Update(Fingers, Geometry, Screen), 0U);
	EXPECT_EQ(Touch.Update({}, Geometry, Screen), 1U << SELECT);
	EXPECT_EQ(Touch.Update({}, Geometry, Screen), 0U);
}

TEST(QmMobileSpectator, InterruptionCancelsPendingPlayerSelector)
{
	CTouchState Touch;
	const auto Geometry = Layout(Screen);
	Touch.Update(FingerAt(Geometry.m_aButtons[SELECT].Center()), Geometry, Screen);
	Touch.Cancel();
	EXPECT_EQ(Touch.Update({}, Geometry, Screen), 0U);
}

TEST(QmMobileSpectator, AutoSpectateWaitsForLocalInfoAndThrottlesRetries)
{
	CJoinState Join;
	EXPECT_FALSE(Join.Update(true, false, false, 1.0));
	EXPECT_TRUE(Join.Update(true, true, false, 1.0));
	EXPECT_FALSE(Join.Update(true, true, false, 2.0));
	EXPECT_TRUE(Join.Update(true, true, false, 4.0));
}

TEST(QmMobileSpectator, SpectatorAcknowledgementAndReconnectResetRetryState)
{
	CJoinState Join;
	EXPECT_TRUE(Join.Update(true, true, false, 1.0));
	EXPECT_FALSE(Join.Update(true, true, true, 2.0));
	EXPECT_TRUE(Join.Update(true, true, false, 2.1));
	EXPECT_FALSE(Join.Update(false, false, false, 2.2));
	EXPECT_TRUE(Join.Update(true, true, false, 2.3));
}

TEST(QmMobileSpectator, GameplayInputIsReleasedWithoutLosingCameraAndChatFlags)
{
	CNetObj_PlayerInput Input{};
	Input.m_Direction = 1;
	Input.m_Jump = Input.m_Hook = 1;
	Input.m_Fire = 63;
	Input.m_NextWeapon = Input.m_PrevWeapon = Input.m_WantedWeapon = 2;
	Input.m_TargetX = 123;
	Input.m_TargetY = 456;
	Input.m_PlayerFlags = PLAYERFLAG_CHATTING | PLAYERFLAG_SPEC_CAM;
	NeutralizeGameplayInput(Input);
	EXPECT_EQ(Input.m_Direction, 0);
	EXPECT_EQ(Input.m_Jump, 0);
	EXPECT_EQ(Input.m_Hook, 0);
	EXPECT_EQ(Input.m_Fire, 0);
	EXPECT_EQ(Input.m_NextWeapon, 0);
	EXPECT_EQ(Input.m_PrevWeapon, 0);
	EXPECT_EQ(Input.m_WantedWeapon, 0);
	EXPECT_EQ(Input.m_TargetX, 123);
	EXPECT_EQ(Input.m_TargetY, 456);
	EXPECT_EQ(Input.m_PlayerFlags, PLAYERFLAG_CHATTING | PLAYERFLAG_SPEC_CAM);
	NeutralizeGameplayInput(Input);
	EXPECT_EQ(Input.m_Fire, 0);
}
