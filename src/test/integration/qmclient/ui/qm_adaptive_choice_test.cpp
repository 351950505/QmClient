// 真实字体测量与下拉生命周期协作；只隔离设备，使用正式选择器准备入口。
#include <engine/keys.h>

#include <game/client/QmUi/SettingsChoice.h>
#include <game/client/QmUi/UiForms.h>

#include <test/support/qm_real_ui_fixture.h>
#include <test/support/qm_ui_test_client.h>
#include <test/support/qm_ui_test_input.h>

namespace
{
	class QmAdaptiveChoice : public qm_ui_test::CRealUiFixture
	{
	protected:
		qm_ui_test::CTestInput m_Input;
		qm_ui_test::CTestUiClient m_Client;
		SSettingsChoiceState m_State;
		IInput *UiInput() override { return &m_Input; }
		IClient *UiClient() override { return &m_Client; }

		void Frame(vec2 Position, bool Pressed)
		{
			const vec2 Pixels = Position * vec2(m_Ui.Graphics()->WindowWidth(), m_Ui.Graphics()->WindowHeight()) / m_Ui.Screen()->Size();
			const vec2 Delta = Pixels - m_Ui.UpdatedMousePos();
			m_Ui.OnCursorMove(Delta.x, Delta.y);
			m_Input.m_HeldKey = Pressed ? KEY_MOUSE_1 : 0;
			m_Client.AdvanceFrame();
			m_Ui.Update();
		}
	};
}

TEST_F(QmAdaptiveChoice, ActualLongTextAndFontSizeChangeThePresentation)
{
	const char *apLabels[] = {"A", "B", "C"};
	CUIRect Controls{40, 40, 300, 20};
	EXPECT_FALSE(PrepareSettingsChoice(m_Ui, m_State, Controls, apLabels, 3, 12).m_UseDropdown);
	apLabels[1] = "A long translated option which must remain distinguishable";
	EXPECT_TRUE(PrepareSettingsChoice(m_Ui, m_State, Controls, apLabels, 3, 12).m_UseDropdown);
	apLabels[1] = "Regular";
	EXPECT_FALSE(PrepareSettingsChoice(m_Ui, m_State, Controls, apLabels, 3, 12).m_UseDropdown);
	EXPECT_TRUE(PrepareSettingsChoice(m_Ui, m_State, Controls, apLabels, 3, 40).m_UseDropdown);
}

TEST_F(QmAdaptiveChoice, GrowingToCapsuleClosesPopupAndDiscardsItsPendingSelection)
{
	const char *apLabels[] = {"White", "Black", "Rainbow"};
	CUIRect Trigger{40, 40, 140, 20};
	Frame({80, 50}, false);
	ASSERT_TRUE(PrepareSettingsChoice(m_Ui, m_State, Trigger, apLabels, 3, 12).m_UseDropdown);
	m_Ui.DoDropDown(&Trigger, 1, apLabels, 3, m_State.m_DropDown);
	Frame({80, 50}, true);
	m_Ui.DoDropDown(&Trigger, 1, apLabels, 3, m_State.m_DropDown);
	Frame({80, 50}, false);
	EXPECT_EQ(m_Ui.DoDropDown(&Trigger, 1, apLabels, 3, m_State.m_DropDown), 1);
	ASSERT_TRUE(m_Ui.IsPopupOpen(&m_State.m_DropDown.m_SelectionPopupContext));
	m_State.m_DropDown.m_SelectionPopupContext.m_SelectionIndex = 2;
	Trigger.w = 400;
	EXPECT_FALSE(PrepareSettingsChoice(m_Ui, m_State, Trigger, apLabels, 3, 12).m_UseDropdown);
	EXPECT_FALSE(m_Ui.IsPopupOpen(&m_State.m_DropDown.m_SelectionPopupContext));
	EXPECT_EQ(m_State.m_DropDown.m_SelectionPopupContext.m_SelectionIndex, -1);
	Trigger.w = 140;
	ASSERT_TRUE(PrepareSettingsChoice(m_Ui, m_State, Trigger, apLabels, 3, 12).m_UseDropdown);
	EXPECT_EQ(m_Ui.DoDropDown(&Trigger, 1, apLabels, 3, m_State.m_DropDown), 1);
}

TEST_F(QmAdaptiveChoice, RenderOnlyPreparationPreservesTheLivePopup)
{
	const char *apLabels[] = {"White", "Black"};
	CUIRect Trigger{40, 40, 100, 20};
	Frame({80, 50}, false);
	m_Ui.DoDropDown(&Trigger, 0, apLabels, 2, m_State.m_DropDown);
	Frame({80, 50}, true);
	m_Ui.DoDropDown(&Trigger, 0, apLabels, 2, m_State.m_DropDown);
	Frame({80, 50}, false);
	m_Ui.DoDropDown(&Trigger, 0, apLabels, 2, m_State.m_DropDown);
	ASSERT_TRUE(m_Ui.IsPopupOpen(&m_State.m_DropDown.m_SelectionPopupContext));
	Trigger.w = 400;
	m_Ui.BeginRenderOnly();
	PrepareSettingsChoice(m_Ui, m_State, Trigger, apLabels, 2, 12);
	m_Ui.EndRenderOnly();
	EXPECT_TRUE(m_Ui.IsPopupOpen(&m_State.m_DropDown.m_SelectionPopupContext));
}

TEST_F(QmAdaptiveChoice, NumericLabelsUseTheSameColumnAsChoiceRows)
{
	for(const float Width : {80.0f, 240.0f, 700.0f, 1000.0f})
	{
		SCOPED_TRACE(Width);
		const auto Metrics = ResolveSettingsContentMetrics(Width);
		const CUIRect Row{10, 20, Width, Metrics.m_LineHeight};
		const auto Choice = ResolveSettingsRadioRowLayout(Row, 3, Metrics);
		const auto NumericLabel = ui_widget::SliderInputFieldLabelRect(Row, true);
		EXPECT_FLOAT_EQ(NumericLabel.x, Choice.m_LabelRect.x);
		EXPECT_FLOAT_EQ(NumericLabel.w, Choice.m_LabelRect.w);
		EXPECT_FLOAT_EQ(NumericLabel.h, Choice.m_LabelRect.h);
	}
}
