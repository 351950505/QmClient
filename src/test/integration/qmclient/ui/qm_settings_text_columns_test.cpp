// 使用真实文字测量验证左右两列协作，GPU 由公共 fixture 隔离。
#include <game/client/QmUi/SettingsCardContentBridge.h>
#include <game/client/QmUi/SettingsContentColumns.h>
#include <game/client/QmUi/SettingsContentLayout.h>
#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/UiButtons.h>

#include <test/support/qm_real_ui_fixture.h>

namespace
{
	class QmSettingsTextColumns : public qm_ui_test::CRealUiFixture
	{
	protected:
		CSettingsContentLayoutCache m_Cache;
		int m_Measurements = 0;
		std::string m_Secondary = "On";
		float m_Fraction = 0.62f;

		std::shared_ptr<const CSettingsContentLayout> Resolve(float Width)
		{
			std::vector<SSettingsContentRowDeclaration> vRows;
			CUIRect Content{0, 0, Width, 0};
			{
				CSettingsContentLayoutScope Scope(vRows);
				CSettingsContentLayoutFlow Flow(Content);
				Flow.TextColumns("preview-setting", "Global UI size", m_Secondary.c_str(), 12, 20, m_Fraction, 4, 4, {}, [](const CUIRect &, const char *) {});
				CUIRect Row;
				Flow.Control("apply", 20, Row);
			}
			return m_Cache.Resolve(vRows, Width, m_Ui.TextRender()->GlyphAtlasRevision(), [this](const char *pText, float TextWidth, float Size) {
				++m_Measurements;
				return m_Ui.TextRender()->TextBoundingBox(Size, pText, -1, TextWidth).m_H + Size * 0.25f;
			});
		}
	};
}

TEST_F(QmSettingsTextColumns, SpecializedBlockDeclaresWithoutExecutingEditorAndRebuildsAfterInput)
{
	SSettingsCardDefinition Definition;
	Definition.m_Spec = {"editor", "Editor", nullptr};
	int Inputs = 0;
	int Renders = 0;
	float Height = 80;
	Definition.m_PreLayoutInput = [&](CUIRect Rect) {
		EXPECT_FLOAT_EQ(Rect.x, 40);
		EXPECT_FLOAT_EQ(Rect.y, 60);
		EXPECT_FLOAT_EQ(Rect.h, 80);
		++Inputs;
		Height = 140;
		return true;
	};
	Definition.m_Render = [&](CUIRect) { ++Renders; };
	qm_settings_content::UseDeclaredBlockContent(Definition, &m_Ui, m_Ui.TextRender(), [&](float) { return Height; });
	auto pLayout = Definition.m_ResolveContentLayout(m_Cache, 300);
	EXPECT_EQ(Inputs, 0);
	EXPECT_EQ(Renders, 0);
	EXPECT_FLOAT_EQ(pLayout->Height(), 80);
	EXPECT_TRUE(qm_settings_content::ExecuteInput(Definition, pLayout, m_Cache, {40, 60, 300, 80}, true, false, false, &m_Ui));
	EXPECT_EQ(Inputs, 1);
	EXPECT_EQ(Renders, 1);
	EXPECT_FLOAT_EQ(pLayout->Height(), 140);
	Definition.m_ContentLayoutDraw(*pLayout, {40, 60, 300, 140});
	EXPECT_EQ(Inputs, 1);
	EXPECT_EQ(Renders, 2);
	EXPECT_EQ(CSettingsContentLayoutScope::Current(), nullptr);
}

TEST_F(QmSettingsTextColumns, SpecializedGeometryDoesNotInheritOuterRowCoordinates)
{
	SSettingsCardDefinition Definition;
	Definition.m_Spec = {"editor", "Editor", nullptr};
	float ActualY = -1;
	Definition.m_Render = [&](CUIRect Rect) {
		CSettingsContentLayoutFlow InternalFlow(Rect);
		CUIRect Row;
		ASSERT_TRUE(InternalFlow.Control("editor-local-row", 20, Row));
		ActualY = Row.y;
	};
	qm_settings_content::UseDeclaredBlockContent(Definition, &m_Ui, m_Ui.TextRender(), [](float) { return 80; });
	const auto pLayout = Definition.m_ResolveContentLayout(m_Cache, 300);
	Definition.m_ContentLayoutDraw(*pLayout, {40, 60, 300, 80});
	EXPECT_FLOAT_EQ(ActualY, 60);
	EXPECT_EQ(CSettingsContentLayoutScope::Current(), nullptr);
}

TEST_F(QmSettingsTextColumns, ControlColumnsWrapLongTranslationAndKeepIndependentOrigins)
{
	SSettingsContentColumnsState State;
	std::vector<SSettingsContentRowDeclaration> vRows;
	float TextY = -1;
	float RightX = -1;
	const auto Left = [](CUIRect &Rect) {
		CSettingsContentLayoutFlow Flow(Rect);
		CUIRect Row;
		Flow.Control("keyboard", 80, Row);
	};
	const auto Right = [&](CUIRect &Rect) {
		CSettingsContentLayoutFlow Flow(Rect);
		Flow.Text("help", "A long explanation that must wrap within the actual editor column and move the action below it", 0, 12, 4, {}, [&](const CUIRect &Row, const char *) { TextY = Row.y; RightX = Row.x; });
		CUIRect Row;
		Flow.Control("action", 20, Row);
	};
	const auto Measure = [&](const char *pText, float Width, float Size) { return m_Ui.TextRender()->TextBoundingBox(Size, pText, -1, Width).m_H + Size * 0.25f; };
	CUIRect Content{0, 0, 300, 0};
	{
		CSettingsContentLayoutScope Scope(vRows);
		RenderSettingsContentColumns("columns", Content, State, 180, 4, false, 1, Measure, {Left, Right});
	}
	const auto pWide = m_Cache.Resolve(vRows, 300, 1, Measure);
	const float RightHeight = State.m_apLayouts[1]->Height();
	EXPECT_GT(RightHeight, 20);
	Content = {40, 60, 300, pWide->Height()};
	{
		CSettingsContentLayoutScope Scope(*pWide, ESettingsContentPass::DRAW);
		RenderSettingsContentColumns("columns", Content, State, 180, 4, false, 1, Measure, {Left, Right});
	}
	EXPECT_FLOAT_EQ(TextY, 60);
	EXPECT_FLOAT_EQ(RightX, 224);
	vRows.clear();
	Content = {0, 0, 160, 0};
	{
		CSettingsContentLayoutScope Scope(vRows);
		RenderSettingsContentColumns("columns", Content, State, 160, 4, true, 1, Measure, {Left, Right});
	}
	const auto pStacked = m_Cache.Resolve(vRows, 160, 1, Measure);
	EXPECT_FLOAT_EQ(pStacked->Height(), 84 + State.m_apLayouts[1]->Height());
	Content = {40, 60, 160, pStacked->Height()};
	{
		CSettingsContentLayoutScope Scope(*pStacked, ESettingsContentPass::DRAW);
		RenderSettingsContentColumns("columns", Content, State, 160, 4, true, 1, Measure, {Left, Right});
	}
	EXPECT_FLOAT_EQ(TextY, 144);
	EXPECT_FLOAT_EQ(RightX, 40);
}

TEST_F(QmSettingsTextColumns, LongSecondaryTranslationMovesFollowingControlAndRecovers)
{
	const auto pShort = Resolve(220);
	m_Secondary = "Current fonts and weights are retained because the recommended family has not been installed.";
	const auto pLong = Resolve(220);
	EXPECT_GT(pLong->Height(), pShort->Height());
	ASSERT_NE(pLong->Find("apply"), nullptr);
	EXPECT_GT(pLong->Find("apply")->m_Rect.y, pShort->Find("apply")->m_Rect.y);
	EXPECT_EQ(Resolve(220), pLong);
	EXPECT_EQ(m_Measurements, 4);
	m_Secondary = "On";
	EXPECT_FLOAT_EQ(Resolve(220)->Height(), pShort->Height());
}

TEST_F(QmSettingsTextColumns, ColumnWidthChangeInvalidatesAndDrawUsesPublishedTexts)
{
	m_Secondary = "A long value that wraps across several lines";
	const auto pWideValue = Resolve(300);
	m_Fraction = 0.85f;
	const auto pNarrowValue = Resolve(300);
	EXPECT_GT(pNarrowValue->Height(), pWideValue->Height());
	std::vector<std::string> vSubmitted;
	std::vector<CUIRect> vRects;
	CUIRect Content{40, 80, 300, pNarrowValue->Height()};
	CSettingsContentLayoutScope Scope(*pNarrowValue, ESettingsContentPass::DRAW);
	CSettingsContentLayoutFlow Flow(Content);
	Flow.TextColumns("preview-setting", "changed label", "changed value", 99, 99, 0.1f, 99, 99, {}, [&](const CUIRect &Row, const char *pText) {
		vRects.push_back(Row);
		vSubmitted.emplace_back(pText);
	});
	ASSERT_EQ(vSubmitted.size(), 2u);
	EXPECT_EQ(vSubmitted[0], "Global UI size");
	EXPECT_EQ(vSubmitted[1], m_Secondary);
	EXPECT_FLOAT_EQ(vRects[0].y, 80);
	EXPECT_FLOAT_EQ(vRects[0].h, vRects[1].h);
	EXPECT_GT(vRects[1].x, vRects[0].x + vRects[0].w);
	EXPECT_LE(vRects[1].x + vRects[1].w, 340);
}

TEST_F(QmSettingsTextColumns, InputDoesNotDrawAndZeroWidthCanRecover)
{
	const auto pEmptyWidth = Resolve(0);
	EXPECT_EQ(m_Measurements, 0);
	{
		CSettingsContentLayoutScope Scope(*pEmptyWidth, ESettingsContentPass::INPUT);
		CUIRect Content{};
		CSettingsContentLayoutFlow Flow(Content);
		Flow.TextColumns("preview-setting", "label", "value", 12, 20, 0.62f, 4, 4, {}, [](const CUIRect &, const char *) { ADD_FAILURE() << "input must not draw"; });
	}
	EXPECT_EQ(Resolve(300)->Rows().size(), 2u);
	EXPECT_EQ(m_Measurements, 2);
}

TEST_F(QmSettingsTextColumns, InputWithTranslatedActionPreservesSpaceAndDoesNotAddRows)
{
	ui_widget::SSettingsActionGroupState Widths;
	const auto Metrics = ResolveSettingsContentMetrics(1000);
	const char *apLabels[] = {"Redeem the sponsor activation code"};
	for(float Width : {0.0f, 4.0f, 80.0f, 600.0f})
	{
		SCOPED_TRACE(Width);
		const CUIRect Row{40, 80, Width, Metrics.m_LineHeight};
		const auto Buttons = ui_widget::PrepareSettingsActionGroup(m_Ui, Widths, Row, apLabels, 1, Metrics);
		const auto Layout = ResolveSettingsInputActionsLayout(Row, 1, Metrics, Buttons.m_aButtons[0].w);
		EXPECT_GE(Layout.m_Input.w, Width * 0.4f);
		EXPECT_LE(Layout.m_Actions.m_aButtons[0].x + Layout.m_Actions.m_aButtons[0].w, Row.x + Row.w + 0.0001f);
		EXPECT_FLOAT_EQ(Layout.m_Input.h, Row.h);
		EXPECT_FLOAT_EQ(Layout.m_Actions.m_aButtons[0].h, Row.h);
	}
}
