#include <engine/client/input_sdl3.h>

#include <gtest/gtest.h>

#include <string>

TEST(Sdl3InputMapping, PreservesOrdinaryKeyboardBindings)
{
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_A), KEY_A);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_RETURN), KEY_RETURN);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_RCTRL), KEY_RCTRL);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MODE), KEY_MODE);
}

TEST(Sdl3InputMapping, PreservesStoredMultimediaKeyNumbers)
{
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_NEXT_TRACK), 258);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_PREVIOUS_TRACK), 259);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_STOP), 260);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_PLAY), 261);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_PLAY_PAUSE), 261);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_SELECT), 263);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_AC_BACK), KEY_AC_BACK);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_EJECT), KEY_EJECT);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_SLEEP), KEY_SLEEP);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_REWIND), KEY_AUDIOREWIND);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_FAST_FORWARD), KEY_AUDIOFASTFORWARD);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_ENDCALL), KEY_ENDCALL);
}

TEST(Sdl3InputMapping, NewOrInvalidScancodesCannotTriggerMouseOrJoystickBindings)
{
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_RECORD), KEY_UNKNOWN);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_MEDIA_PAUSE), KEY_UNKNOWN);
	EXPECT_EQ(SdlScancodeToKey(KEY_MOUSE_1), KEY_UNKNOWN);
	EXPECT_EQ(SdlScancodeToKey(-1), KEY_UNKNOWN);
	EXPECT_EQ(SdlScancodeToKey(SDL_SCANCODE_COUNT), KEY_UNKNOWN);
}

TEST(Sdl3TextInput, LongAsciiCommitPreservesAllTextAndEventMetadata)
{
	std::vector<IInput::CEvent> vEvents;
	const std::string Text(100, 'x');
	AppendSdlTextEvents(vEvents, Text.c_str(), 42);
	ASSERT_EQ(vEvents.size(), 4u);
	std::string Result;
	for(const auto &Event : vEvents)
	{
		EXPECT_EQ(Event.m_Flags, IInput::FLAG_TEXT);
		EXPECT_EQ(Event.m_Key, KEY_UNKNOWN);
		EXPECT_EQ(Event.m_InputCount, 42u);
		Result += Event.m_aText;
	}
	EXPECT_EQ(Result, Text);
}

TEST(Sdl3TextInput, MultibyteCommitOnlySplitsBetweenCompleteCodepoints)
{
	std::vector<IInput::CEvent> vEvents;
	const std::string Text = std::string(29, 'a') + "中文🙂输入法提交超过旧事件长度";
	AppendSdlTextEvents(vEvents, Text.c_str(), 1);
	ASSERT_GT(vEvents.size(), 1u);
	EXPECT_STREQ(vEvents.front().m_aText, std::string(29, 'a').c_str());
	std::string Result;
	for(const auto &Event : vEvents)
	{
		EXPECT_TRUE(str_utf8_check(Event.m_aText));
		EXPECT_LT(str_length(Event.m_aText), 32);
		Result += Event.m_aText;
	}
	EXPECT_EQ(Result, Text);
}

TEST(Sdl3TextInput, ExactCapacityDoesNotAddAnEmptyTrailingEvent)
{
	std::vector<IInput::CEvent> vEvents;
	const std::string Text(31, 'x');
	AppendSdlTextEvents(vEvents, Text.c_str(), 1);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_STREQ(vEvents.front().m_aText, Text.c_str());
}

TEST(Sdl3TextInput, EmptyCommitStillNotifiesTextConsumers)
{
	std::vector<IInput::CEvent> vEvents;
	AppendSdlTextEvents(vEvents, "", 1);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_STREQ(vEvents.front().m_aText, "");
	EXPECT_EQ(vEvents.front().m_Flags, IInput::FLAG_TEXT);
}
