// 真实 UI 帧、输入框与文字引擎协作，只替换时钟和输入设备。
#include <engine/keys.h>

#include <game/client/QmUi/UiForms.h>

#include <test/support/qm_real_ui_fixture.h>
#include <test/support/qm_ui_test_client.h>
#include <test/support/qm_ui_test_input.h>

namespace
{
	class QmModernTextInput : public qm_ui_test::CRealUiFixture
	{
	protected:
		qm_ui_test::CTestUiClient m_Client;
		qm_ui_test::CTestInput m_Input;
		CLineInputBuffered<128> m_Field;
		CUIRect m_Rect{10.0f, 10.0f, 220.0f, 30.0f};

		IInput *UiInput() override { return &m_Input; }
		IClient *UiClient() override { return &m_Client; }

		void SetUp() override
		{
			CRealUiFixture::SetUp();
			m_Field.Set("modern 中文 input");
			MoveMouse({40.0f, 25.0f});
		}

		void MoveMouse(vec2 Position)
		{
			const vec2 Pixels = Position * vec2(m_Ui.Graphics()->WindowWidth(), m_Ui.Graphics()->WindowHeight()) / m_Ui.Screen()->Size();
			const vec2 Delta = Pixels - m_Ui.UpdatedMousePos();
			m_Ui.OnCursorMove(Delta.x, Delta.y);
		}

		void Draw(bool Multiline = false)
		{
			if(Multiline)
				m_Ui.DoEditBoxMultiLine(&m_Field, &m_Rect, 12.0f, 1.0f);
			else
				m_Ui.DoEditBox(&m_Field, &m_Rect, 12.0f);
		}

		void Frame(bool MouseDown, bool Multiline = false, bool RepeatDraw = false)
		{
			m_Input.m_HeldKey = MouseDown ? KEY_MOUSE_1 : 0;
			m_Client.AdvanceFrame();
			m_Ui.StartCheck();
			m_Ui.Update();
			Draw(Multiline);
			if(RepeatDraw)
				Draw(Multiline);
			m_Ui.FinishCheck();
		}

		void DoubleClick(bool Multiline = false, bool RepeatDraw = false)
		{
			Frame(false, Multiline);
			Frame(true, Multiline, RepeatDraw);
			Frame(false, Multiline);
			Frame(true, Multiline, RepeatDraw);
			Frame(false, Multiline);
		}
	};
}

TEST_F(QmModernTextInput, DoubleClickSelectsAllAndKeepsSelectionOnRelease)
{
	DoubleClick();
	EXPECT_EQ(m_Field.GetSelectionStart(), 0u);
	EXPECT_EQ(m_Field.GetSelectionEnd(), m_Field.GetLength());
	EXPECT_TRUE(m_Field.IsActive());
	EXPECT_FALSE(m_Field.GetMouseSelection()->m_Selecting);
}

TEST_F(QmModernTextInput, DoubleClickKeepsPasswordMasked)
{
	m_Field.SetHidden(true);
	DoubleClick();
	EXPECT_EQ(m_Field.GetSelectionLength(), m_Field.GetLength());
	EXPECT_TRUE(m_Field.IsHidden());
	EXPECT_STRNE(m_Field.GetDisplayedString(), m_Field.GetString());
	EXPECT_EQ(str_length(m_Field.GetDisplayedString()), m_Field.GetNumChars());
}

TEST_F(QmModernTextInput, RenderOnlySharedFormPreservesSelectionAndPendingChange)
{
	IUiContext Context;
	Context.m_pUi = &m_Ui;
	Context.m_pTextRender = m_Ui.TextRender();
	ui_widget::SInputFieldOptions Options;
	m_Ui.SetActiveItem(&m_Field);
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	ASSERT_TRUE(m_Field.IsActive());
	m_Field.Append(" added");
	m_Field.SelectAll();
	m_Field.GetMouseSelection()->m_Selecting = true;
	m_Ui.BeginRenderOnly();
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	m_Ui.EndRenderOnly();
	EXPECT_TRUE(m_Field.IsActive());
	EXPECT_EQ(m_Field.GetSelectionStart(), 0u);
	EXPECT_EQ(m_Field.GetSelectionEnd(), m_Field.GetLength());
	EXPECT_TRUE(m_Field.GetMouseSelection()->m_Selecting);
	EXPECT_TRUE(m_Field.WasChanged());
}

TEST_F(QmModernTextInput, MultipleDrawPassesDoNotTurnOnePressIntoADoubleClick)
{
	Frame(false);
	Frame(true, false, true);
	Frame(false);
	EXPECT_LT(m_Field.GetSelectionLength(), m_Field.GetLength());
	Frame(true, false, true);
	Frame(false);
	EXPECT_EQ(m_Field.GetSelectionLength(), m_Field.GetLength());
}

TEST_F(QmModernTextInput, SingleClickAndDragStillReplaceTheDoubleClickSelection)
{
	DoubleClick();
	MoveMouse({25.0f, 25.0f});
	Frame(true);
	MoveMouse({65.0f, 25.0f});
	Frame(true);
	Frame(false);
	EXPECT_GT(m_Field.GetSelectionLength(), 0u);
	EXPECT_LT(m_Field.GetSelectionLength(), m_Field.GetLength());
}

TEST_F(QmModernTextInput, MultilineDoubleClickUsesTheSameSelectAllBehavior)
{
	m_Field.Set("first line\nsecond line");
	m_Rect.h = 60.0f;
	DoubleClick(true, true);
	EXPECT_EQ(m_Field.GetSelectionStart(), 0u);
	EXPECT_EQ(m_Field.GetSelectionEnd(), m_Field.GetLength());
	EXPECT_TRUE(m_Field.IsActive());
}

TEST_F(QmModernTextInput, ImeConsumedEnterKeepsSharedFormActiveAndUnsubmitted)
{
	IUiContext Context;
	Context.m_pUi = &m_Ui;
	Context.m_pTextRender = m_Ui.TextRender();
	ui_widget::SInputFieldOptions Options;
	m_Ui.SetActiveItem(&m_Field);
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	ASSERT_TRUE(m_Field.IsActive());
	m_Input.m_Composing = true;
	m_Input.m_RawPressedKey = KEY_RETURN;
	IInput::CEvent Event{};
	Event.m_Flags = IInput::FLAG_PRESS;
	Event.m_Key = KEY_RETURN;
	ASSERT_TRUE(m_Ui.OnInput(Event));
	const auto Result = ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	EXPECT_TRUE(m_Field.IsActive());
	EXPECT_FALSE(Result.m_Deactivated);
	EXPECT_FALSE(Result.m_Submitted);
	EXPECT_FALSE(m_Ui.HotkeyPressed(CUi::HOTKEY_ENTER));
}

TEST_F(QmModernTextInput, ImeEnterAndOutsideClickInOneFrameCommitWithoutReportingSubmit)
{
	IUiContext Context;
	Context.m_pUi = &m_Ui;
	Context.m_pTextRender = m_Ui.TextRender();
	ui_widget::SInputFieldOptions Options;
	m_Ui.SetActiveItem(&m_Field);
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	ASSERT_TRUE(m_Field.IsActive());
	m_Input.m_Composing = true;
	m_Input.m_RawPressedKey = KEY_RETURN;
	IInput::CEvent Event{};
	Event.m_Flags = IInput::FLAG_PRESS;
	Event.m_Key = KEY_RETURN;
	ASSERT_TRUE(m_Ui.OnInput(Event));
	MoveMouse({300.0f, 25.0f});
	m_Input.m_HeldKey = KEY_MOUSE_1;
	m_Client.AdvanceFrame();
	m_Ui.StartCheck();
	m_Ui.Update();
	const auto Result = ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	m_Ui.FinishCheck();
	EXPECT_FALSE(m_Field.IsActive());
	EXPECT_TRUE(Result.m_Deactivated);
	EXPECT_TRUE(Result.m_Committed);
	EXPECT_FALSE(Result.m_Submitted);
}

TEST_F(QmModernTextInput, RoutedEnterReportsSubmitAndRemainsAvailableToTheCaller)
{
	IUiContext Context;
	Context.m_pUi = &m_Ui;
	Context.m_pTextRender = m_Ui.TextRender();
	ui_widget::SInputFieldOptions Options;
	m_Ui.SetActiveItem(&m_Field);
	ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	ASSERT_TRUE(m_Field.IsActive());
	IInput::CEvent Event{};
	Event.m_Flags = IInput::FLAG_PRESS;
	Event.m_Key = KEY_RETURN;
	ASSERT_TRUE(m_Ui.OnInput(Event));
	const auto Result = ui_widget::InputField(Context, &m_Field, m_Rect, Options);
	EXPECT_FALSE(m_Field.IsActive());
	EXPECT_TRUE(Result.m_Submitted);
	EXPECT_TRUE(m_Ui.ConsumeHotkey(CUi::HOTKEY_ENTER));
}
