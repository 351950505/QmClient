#include <game/client/components/qmclient/bindwheel_state.h>

#include <gtest/gtest.h>

TEST(QmBindWheelState, ReleaseCommitsSelectionOnce)
{
	CQmBindWheelState State;
	State.Open();
	State.Release(3);
	State.Release(4);
	EXPECT_FALSE(State.IsActive());
	EXPECT_EQ(State.TakePendingBind(), 3);
	EXPECT_FALSE(State.TakePendingBind().has_value());
}

TEST(QmBindWheelState, EscapeAndGlobalReleaseCancelSelection)
{
	CQmBindWheelState State;
	State.Open();
	State.Cancel();
	State.Release(2);
	EXPECT_FALSE(State.IsActive());
	EXPECT_FALSE(State.TakePendingBind().has_value());
}

TEST(QmBindWheelState, CancelDiscardsPendingCommand)
{
	CQmBindWheelState State;
	State.Open();
	State.Release(2);
	State.Cancel();
	EXPECT_FALSE(State.TakePendingBind().has_value());
}

TEST(QmBindWheelState, BindMutationDiscardsPendingIndex)
{
	CQmBindWheelState State;
	State.Open();
	State.Release(2);
	State.InvalidateSelection();
	EXPECT_FALSE(State.TakePendingBind().has_value());
}

TEST(QmBindWheelState, ReopeningDoesNotExecutePreviousSelection)
{
	CQmBindWheelState State;
	State.Open();
	State.Release(2);
	State.Open();
	EXPECT_TRUE(State.IsActive());
	EXPECT_FALSE(State.TakePendingBind().has_value());
}

TEST(QmBindWheelState, ResolvesLatestCursorPositionAndEmptyCenter)
{
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(0, 0), 4), -1);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(110, 0), 4), -1);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(170, 0), 0), -1);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(170, 0), 4), 0);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(0, 170), 4), 1);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(-170, 0), 4), 2);
	EXPECT_EQ(CQmBindWheelState::SelectedBind(vec2(0, -170), 4), 3);
}

TEST(QmBindWheelState, CenterReleaseStillRequestsMouseReset)
{
	CQmBindWheelState State;
	State.Open();
	State.Release(-1);
	const auto Pending = State.TakePendingBind();
	ASSERT_TRUE(Pending.has_value());
	EXPECT_EQ(*Pending, -1);
}
