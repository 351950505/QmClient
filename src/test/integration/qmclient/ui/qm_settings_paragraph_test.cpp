#include <game/client/QmUi/SettingsParagraph.h>

#include <test/support/qm_real_ui_fixture.h>

namespace
{
	class QmSettingsParagraph : public qm_ui_test::CRealUiFixture
	{
	};
}

TEST_F(QmSettingsParagraph, MeasuredWrappedParagraphContainsRenderedLinesWithSpacing)
{
	SSettingsParagraphState State;
	const char *pNames = "Alice, Bob, Carol, Daniel, Evelyn, Franklin, Grace, Harold";
	for(float Width : {90.0f, 180.0f})
	{
		for(float Spacing : {0.0f, 4.0f})
		{
			const float Height = MeasureSettingsParagraph(m_Ui.TextRender(), pNames, Width, 14.0f, Spacing);
			DrawSettingsParagraph(&m_Ui, State, {10, 20, Width, Height}, pNames, 14.0f, Spacing);
			const auto &Cursor = State.m_Element.Rect(0)->m_Cursor;
			EXPECT_GT(Cursor.m_LineCount, 1);
			EXPECT_GE(Height, Cursor.Height());
			EXPECT_FLOAT_EQ(Cursor.m_LineSpacing, Spacing);
			EXPECT_GT(Cursor.m_GlyphCount, 0);
		}
	}
}

TEST_F(QmSettingsParagraph, RepeatedDrawReusesContainerAndNewSpacingRebuildsLayout)
{
	SSettingsParagraphState State;
	const CUIRect Rect{0, 0, 80, 200};
	const char *pText = "Alice, Bob, Carol, Daniel";
	DrawSettingsParagraph(&m_Ui, State, Rect, pText, 14, 0);
	const float BeforeHeight = State.m_Element.Rect(0)->m_Cursor.Height();
	DrawSettingsParagraph(&m_Ui, State, Rect, pText, 14, 4);
	EXPECT_GT(State.m_Element.Rect(0)->m_Cursor.Height(), BeforeHeight);
	const auto Before = m_Ui.TextRender()->QmTextRuntimeBudgetSnapshot().m_TextContainerNew;
	DrawSettingsParagraph(&m_Ui, State, Rect, pText, 14, 4);
	EXPECT_EQ(m_Ui.TextRender()->QmTextRuntimeBudgetSnapshot().m_TextContainerNew, Before);
}
