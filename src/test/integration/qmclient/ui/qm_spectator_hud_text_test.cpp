// 观战 HUD 布局与真实文字引擎协作，验证完整标签和真实溢出。
#include <game/client/components/qmclient/spectator_hud_layout.h>
#include <game/client/components/qmclient/spectator_hud_text.h>

#include <test/support/qm_real_ui_fixture.h>

namespace
{
	class QmSpectatorHudText : public qm_ui_test::CRealUiFixture
	{
	protected:
		CLabelResult Draw(float ViewWidth, const char *pText)
		{
			const float Width = m_Ui.TextRender()->TextWidth(8.0f, pText);
			const auto Layout = QmSpectatorHudLayout(ViewWidth, Width);
			const CUIRect Label{Layout.m_Left + 6.0f, 20.3f, Layout.m_TextWidth, 15.0f};
			return QmDrawSpectatorHudLabel(m_Ui, Label, pText, Width);
		}
	};
}

TEST_F(QmSpectatorHudText, CompleteLocalizedLabelsFitTheirMeasuredWidthAtFractionalScales)
{
	auto *pGraphics = static_cast<qm_ui_test::CRealUiTestGraphics *>(m_Ui.Graphics());
	pGraphics->SetFramebufferSize(1280, 720);
	for(float Scale : {1.0f, 1.25f, 1.7f})
	{
		SCOPED_TRACE(Scale);
		pGraphics->MapScreen(0, 0, 400.0f * Scale, 225.0f * Scale);
		for(const char *pText : {"自由视角", "Free-View", "Multi-View", "Following (测试玩家)"})
		{
			SCOPED_TRACE(pText);
			m_Ui.SetCardLabelHintsEnabled(true);
			EXPECT_FALSE(Draw(400.0f * Scale, pText).m_Truncated);
			EXPECT_TRUE(m_Ui.CardLabelHintsEnabled());
		}
	}
}

TEST_F(QmSpectatorHudText, RealOverflowEllipsizesAndFullLabelReturnsAfterResize)
{
	const char *pText = "Following 一个名字较长的测试玩家";
	EXPECT_TRUE(Draw(40.0f, pText).m_Truncated);
	EXPECT_FALSE(Draw(400.0f, pText).m_Truncated);
	EXPECT_TRUE(Draw(40.0f, pText).m_Truncated);
}

TEST_F(QmSpectatorHudText, EmptyViewportSubmitsNoText)
{
	auto *pGraphics = static_cast<qm_ui_test::CRealUiTestGraphics *>(m_Ui.Graphics());
	pGraphics->ResetCounters();
	QmDrawSpectatorHudLabel(m_Ui, {0, 0, 0, 15}, "自由视角", 32.0f);
	EXPECT_EQ(pGraphics->m_Quads, 0u);
}
