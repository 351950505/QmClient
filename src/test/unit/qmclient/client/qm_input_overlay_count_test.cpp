#include <game/client/components/qmclient/input_overlay.h>

#include <gtest/gtest.h>

namespace
{
	IInput::CEvent KeyEvent(int Key, int Flags)
	{
		IInput::CEvent Event{};
		Event.m_Key = Key;
		Event.m_Flags = Flags;
		return Event;
	}
}

TEST(QmInputOverlayCounts, HeldKeyAndRepeatsOnlyCountOnceUntilReleased)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS | IInput::FLAG_REPEAT), true, true);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 1u);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_RELEASE), true, true);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 2u);
	EXPECT_EQ(Counter.Count(KEY_D), 0u);
}

TEST(QmInputOverlayCounts, NativeMouseButtonsMatchPhysicalEventKeys)
{
	const int aExpectedKeys[] = {KEY_MOUSE_1, KEY_MOUSE_3, KEY_MOUSE_2, KEY_MOUSE_4, KEY_MOUSE_5, KEY_MOUSE_6, KEY_MOUSE_7, KEY_MOUSE_8, KEY_MOUSE_9};
	for(int MouseButton = 1; MouseButton <= NUM_MOUSE_BUTTONS; ++MouseButton)
	{
		SCOPED_TRACE(MouseButton);
		EXPECT_EQ(QmInputOverlay::NativeMouseButtonKey(MouseButton), aExpectedKeys[MouseButton - 1]);
	}
	EXPECT_EQ(QmInputOverlay::NativeMouseButtonKey(-1), KEY_UNKNOWN);
	EXPECT_EQ(QmInputOverlay::NativeMouseButtonKey(0), KEY_UNKNOWN);
	EXPECT_EQ(QmInputOverlay::NativeMouseButtonKey(NUM_MOUSE_BUTTONS + 1), KEY_UNKNOWN);
}

TEST(QmInputOverlayCounts, RightAndMiddleMouseClicksKeepSeparateCountsAfterRelease)
{
	QmInputOverlay::CKeyPressCounter Counter;
	const int RightMouseKey = QmInputOverlay::NativeMouseButtonKey(3);
	const int MiddleMouseKey = QmInputOverlay::NativeMouseButtonKey(2);
	Counter.Observe(KeyEvent(KEY_MOUSE_2, IInput::FLAG_PRESS), true, true);
	Counter.Observe(KeyEvent(KEY_MOUSE_2, IInput::FLAG_RELEASE), true, true);
	EXPECT_EQ(Counter.Count(RightMouseKey), 1u);
	EXPECT_EQ(Counter.Count(MiddleMouseKey), 0u);

	Counter.Observe(KeyEvent(KEY_MOUSE_3, IInput::FLAG_PRESS), true, true);
	Counter.Observe(KeyEvent(KEY_MOUSE_3, IInput::FLAG_RELEASE), true, true);
	EXPECT_EQ(Counter.Count(RightMouseKey), 1u);
	EXPECT_EQ(Counter.Count(MiddleMouseKey), 1u);

	Counter.Observe(KeyEvent(KEY_MOUSE_2, IInput::FLAG_PRESS), true, true);
	Counter.Observe(KeyEvent(KEY_MOUSE_2, IInput::FLAG_RELEASE), true, true);
	EXPECT_EQ(Counter.Count(RightMouseKey), 2u);
	EXPECT_EQ(Counter.Count(MiddleMouseKey), 1u);
}

TEST(QmInputOverlayCounts, DisabledOrUnfocusedInputDoesNotBecomeANewPressWhenResumed)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), false, false);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS | IInput::FLAG_REPEAT), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 0u);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_RELEASE), false, false);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_RELEASE), false, false);
	EXPECT_EQ(Counter.Count(KEY_A), 1u);
}

TEST(QmInputOverlayCounts, ResetClearsCountsWithoutCountingAHeldKeyAgain)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_MOUSE_1, IInput::FLAG_PRESS), true, true);
	Counter.ResetCounts();
	EXPECT_EQ(Counter.Count(KEY_MOUSE_1), 0u);
	Counter.Observe(KeyEvent(KEY_MOUSE_1, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_MOUSE_1), 0u);
	Counter.Observe(KeyEvent(KEY_MOUSE_1, IInput::FLAG_RELEASE), true, true);
	Counter.Observe(KeyEvent(KEY_MOUSE_1, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_MOUSE_1), 1u);
}

TEST(QmInputOverlayCounts, KeyOpeningAnInputInterfaceIsExcludedAndStillTrackedAsHeld)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_T, IInput::FLAG_PRESS), true, false);
	Counter.Observe(KeyEvent(KEY_T, IInput::FLAG_PRESS | IInput::FLAG_REPEAT), true, true);
	EXPECT_EQ(Counter.Count(KEY_T), 0u);
	Counter.Observe(KeyEvent(KEY_T, IInput::FLAG_RELEASE), true, true);
	Counter.Observe(KeyEvent(KEY_T, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_T), 1u);
}

TEST(QmInputOverlayCounts, KeyClosingAnInputInterfaceIsExcludedUntilPressedAgain)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_ESCAPE, IInput::FLAG_PRESS), false, true);
	Counter.Observe(KeyEvent(KEY_ESCAPE, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_ESCAPE), 0u);
	Counter.Observe(KeyEvent(KEY_ESCAPE, IInput::FLAG_RELEASE), true, true);
	Counter.Observe(KeyEvent(KEY_ESCAPE, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_ESCAPE), 1u);
}

TEST(QmInputOverlayCounts, ContextResetPreservesSessionCountsAndIgnoresOrphanRepeat)
{
	QmInputOverlay::CKeyPressCounter Counter;
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	Counter.ReleaseKeys();
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS | IInput::FLAG_REPEAT), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 1u);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_RELEASE), true, true);
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_PRESS), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 2u);
}

TEST(QmInputOverlayCounts, CombinedPressReleaseEventsCountEachPhysicalAction)
{
	QmInputOverlay::CKeyPressCounter Counter;
	const auto Event = KeyEvent(KEY_MOUSE_WHEEL_UP, IInput::FLAG_PRESS | IInput::FLAG_RELEASE);
	Counter.Observe(Event, true, true);
	Counter.Observe(Event, true, true);
	EXPECT_EQ(Counter.Count(KEY_MOUSE_WHEEL_UP), 2u);
}

TEST(QmInputOverlayCounts, InvalidKeysAndTextEventsDoNotChangeCounts)
{
	QmInputOverlay::CKeyPressCounter Counter;
	const int aInvalidKeys[] = {-1, KEY_UNKNOWN, KEY_LAST};
	for(int Key : aInvalidKeys)
	{
		Counter.Observe(KeyEvent(Key, IInput::FLAG_PRESS), true, true);
		EXPECT_EQ(Counter.Count(Key), 0u);
	}
	Counter.Observe(KeyEvent(KEY_A, IInput::FLAG_TEXT), true, true);
	EXPECT_EQ(Counter.Count(KEY_A), 0u);
}
