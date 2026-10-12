#include <game/client/QmUi/SettingsSearchHighlight.h>

#include <test/support/qm_real_ui_fixture.h>

namespace
{
	class QmSettingsSearchHighlight : public qm_ui_test::CRealUiFixture
	{
	protected:
		CSettingsSearchHighlight m_Highlight;
		CUIRect m_Rect{10.0f, 20.0f, 150.0f, 24.0f};

		void SetUp() override
		{
			CRealUiFixture::SetUp();
			m_Highlight.SetTargets({{"Scale"}});
		}

		void Draw(const char *pLabel = "Scale")
		{
			m_Ui.DoLabel(&m_Rect, pLabel, 12.0f, TEXTALIGN_ML);
		}
	};
}

TEST_F(QmSettingsSearchHighlight, ImmediateLabelCollectsItsLayoutRectOnlyInsideTheScope)
{
	Draw();
	EXPECT_TRUE(m_Highlight.Matches().empty());
	{
		CUiScopedSettingsSearchHighlight Scope(&m_Ui, &m_Highlight);
		Draw();
		ASSERT_EQ(m_Highlight.Matches().size(), 1u);
		EXPECT_FLOAT_EQ(m_Highlight.Matches()[0].m_Rect.y, m_Rect.y);
	}
	EXPECT_EQ(m_Ui.SettingsSearchHighlight(), nullptr);
	m_Highlight.ClearMatches();
	Draw();
	EXPECT_TRUE(m_Highlight.Matches().empty());
}

TEST_F(QmSettingsSearchHighlight, NestedAndSuppressedScopesRestoreThePreviousCollector)
{
	CSettingsSearchHighlight Inner;
	Inner.SetTargets({{"Scale"}});
	CUiScopedSettingsSearchHighlight OuterScope(&m_Ui, &m_Highlight);
	{
		CUiScopedSettingsSearchHighlight InnerScope(&m_Ui, &Inner);
		Draw();
		EXPECT_EQ(Inner.Matches().size(), 1u);
		EXPECT_TRUE(m_Highlight.Matches().empty());
	}
	{
		CUiScopedSettingsSearchHighlight SuppressScope(&m_Ui, nullptr);
		Draw();
		EXPECT_TRUE(m_Highlight.Matches().empty());
	}
	Draw();
	EXPECT_EQ(m_Highlight.Matches().size(), 1u);
}

TEST_F(QmSettingsSearchHighlight, RenderOnlyWarmupDoesNotCollectMatches)
{
	CUiScopedSettingsSearchHighlight Scope(&m_Ui, &m_Highlight);
	m_Ui.BeginRenderOnly();
	Draw();
	m_Ui.EndRenderOnly();
	EXPECT_TRUE(m_Highlight.Matches().empty());
	Draw();
	EXPECT_EQ(m_Highlight.Matches().size(), 1u);
}

TEST_F(QmSettingsSearchHighlight, PrelayoutCanCollectRowsBeyondTheCurrentClip)
{
	CUiScopedSettingsSearchHighlight Scope(&m_Ui, &m_Highlight);
	const CUIRect Clip{0.0f, 0.0f, 200.0f, 100.0f};
	m_Rect.y = 1800.0f;
	m_Ui.BeginPreLayoutInput();
	m_Ui.ClipEnable(&Clip);
	Draw();
	m_Ui.ClipDisable();
	m_Ui.EndPreLayoutInput();
	ASSERT_EQ(m_Highlight.Matches().size(), 1u);
	EXPECT_FLOAT_EQ(m_Highlight.Matches()[0].m_Rect.y, 1800.0f);
}

TEST_F(QmSettingsSearchHighlight, ReusedStreamedLabelUsesCurrentRectWithoutRebuilding)
{
	CUIElement Element;
	Element.Init(&m_Ui, 1);
	CUiScopedSettingsSearchHighlight Scope(&m_Ui, &m_Highlight);
	m_Ui.DoLabelStreamed(*Element.Rect(0), &m_Rect, "Scale", 12.0f, TEXTALIGN_ML, {}, -1, nullptr, false);
	EXPECT_TRUE(m_Highlight.Matches().empty());
	m_Rect.y = 60.0f;
	bool Recreated = true;
	m_Ui.DoLabelStreamed(*Element.Rect(0), &m_Rect, "Scale", 12.0f, TEXTALIGN_ML, {}, -1, nullptr, true, &Recreated);
	EXPECT_FALSE(Recreated);
	ASSERT_EQ(m_Highlight.Matches().size(), 1u);
	EXPECT_FLOAT_EQ(m_Highlight.Matches()[0].m_Rect.y, 60.0f);
	m_Highlight.ClearMatches();
	m_Rect.y = 90.0f;
	m_Ui.RenderLabelTextContainerAligned(*Element.Rect(0), &m_Rect, TEXTALIGN_ML);
	ASSERT_EQ(m_Highlight.Matches().size(), 1u);
	EXPECT_FLOAT_EQ(m_Highlight.Matches()[0].m_Rect.y, 90.0f);
}

TEST_F(QmSettingsSearchHighlight, ConfigTooltipEntryCollectsTheFullMatchedRow)
{
	int Scale = 100;
	int Other = 24;
	m_Highlight.SetTargets({{"UI scale", &Scale}});
	CUiScopedSettingsSearchHighlight Scope(&m_Ui, &m_Highlight);
	m_Ui.DoConfigTooltip(&Other, &m_Rect, &Other);
	EXPECT_TRUE(m_Highlight.Matches().empty());
	m_Ui.DoConfigTooltip(&Other, &m_Rect, &Scale);
	ASSERT_EQ(m_Highlight.Matches().size(), 1u);
	EXPECT_FLOAT_EQ(m_Highlight.Matches()[0].m_Rect.w, m_Rect.w);
	m_Highlight.ClearMatches();
	m_Ui.BeginRenderOnly();
	m_Ui.DoConfigTooltip(&Other, &m_Rect, &Scale);
	m_Ui.EndRenderOnly();
	EXPECT_TRUE(m_Highlight.Matches().empty());
}
