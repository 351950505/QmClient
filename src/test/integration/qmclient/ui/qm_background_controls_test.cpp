// 真实公共组件与文字引擎协作，仅在 GPU 边界记录外观提交。
#include <engine/keys.h>

#include <game/client/QmUi/QmAnimResolve.h>
#include <game/client/QmUi/UiButtons.h>
#include <game/client/QmUi/UiForms.h>

#include <test/support/qm_real_ui_fixture.h>
#include <test/support/qm_ui_test_client.h>
#include <test/support/qm_ui_test_input.h>

#include <vector>

namespace
{
	class CBackgroundGraphics : public CIconBenchmarkGraphics
	{
	public:
		std::vector<SRoundedRectSdfParams> m_vSurfaces;
		bool HasRoundedRectSdf() override { return true; }
		void ClipEnable(int, int, int, int) override {}
		void ClipDisable() override {}
		void RenderRoundedRectSdf(const SRoundedRectSdfParams &Params) override { m_vSurfaces.push_back(Params); }
	};

	class QmBackgroundControlsIntegration : public qm_ui_test::CRealUiFixture
	{
	protected:
		CBackgroundGraphics m_Recording;
		qm_ui_test::CTestInput m_Input;
		qm_ui_test::CTestUiClient m_Client;
		std::unique_ptr<qm_ui_test::CRealUiTestKernel> m_pRecordingKernel;
		CButtonContainer m_Button;
		CLineInputBuffered<64> m_Text;
		CLineInputNumber m_Integer;
		ui_widget::SNumericFieldState m_Number;
		int m_Value = 123;
		bool m_Toggle = true;
		CUIRect m_Rect{20, 20, 280, 24};

		IInput *UiInput() override { return &m_Input; }
		IClient *UiClient() override { return &m_Client; }
		void SetUp() override
		{
			CRealUiFixture::SetUp();
			if(HasFatalFailure())
				return;
			g_Config.m_QmUiAccentOpacity = 60;
			g_Config.m_QmUiDropdownOpacity = 40;
			g_Config.m_QmUiInputOpacity = 60;
			m_pRecordingKernel = std::make_unique<qm_ui_test::CRealUiTestKernel>(&m_Recording, m_Ui.TextRender(), &m_Input, &m_Client);
			m_Ui.Init(m_pRecordingKernel.get());
			m_Text.Set("server filter");
			m_Text.SetEmptyText("original placeholder");
			m_Text.SetCursorOffset(5);
			m_Text.SetSelection(2, 5);
			m_Integer.Set("pending integer");
			m_Number.m_Input.Set("pending numeric");
		}

		void DrawControls()
		{
			const auto Ctx = ui_widget::ControlContext(&m_Ui);
			EXPECT_FALSE(ui_widget::PrimaryButton(Ctx, &m_Button, "Connect", m_Rect));
			ui_widget::SInputFieldOptions InputOptions;
			InputOptions.m_pPlaceholder = "replacement";
			EXPECT_FALSE(ui_widget::InputField(Ctx, &m_Text, m_Rect, InputOptions).m_Changed);
			EXPECT_FALSE(ui_widget::IntegerField(Ctx, &m_Integer, &m_Value, 0, 100, m_Rect, {}).m_Changed);
			EXPECT_FALSE(ui_widget::NumericField(Ctx, &m_Number, &m_Button, &m_Value, 0, 100, m_Rect, {}));
			EXPECT_FALSE(ui_widget::Toggle(Ctx, &m_Toggle, &m_Toggle, m_Rect));
		}

		void DrawOpacityControls(bool On = false)
		{
			m_Recording.m_vSurfaces.clear();
			g_Config.m_QmUiDropdownOpacity = 40;
			g_Config.m_QmUiInputOpacity = 60;
			const auto Ctx = ui_widget::ControlContext(&m_Ui);
			m_Ui.BeginBackgroundRender();
			ui_widget::DrawButtonSurface(Ctx, &m_Button, m_Rect, {});
			ui_widget::InputField(Ctx, &m_Text, m_Rect, ui_widget::SInputFieldOptions{});
			ui_widget::DrawToggle(Ctx, &m_Toggle, On, m_Rect, true, false);
			m_Ui.EndBackgroundRender();
		}

		void ExpectUnchanged()
		{
			EXPECT_EQ(m_Value, 123);
			EXPECT_TRUE(m_Toggle);
			EXPECT_STREQ(m_Text.GetString(), "server filter");
			EXPECT_STREQ(m_Text.GetEmptyText(), "original placeholder");
			EXPECT_EQ(m_Text.GetCursorOffset(), 5u);
			EXPECT_EQ(m_Text.GetSelectionStart(), 2u);
			EXPECT_EQ(m_Text.GetSelectionEnd(), 5u);
			EXPECT_STREQ(m_Integer.GetString(), "pending integer");
			EXPECT_STREQ(m_Number.m_Input.GetString(), "pending numeric");
			EXPECT_FALSE(m_Number.m_HasSyncedValue);
		}
	};
}

TEST_F(QmBackgroundControlsIntegration, BackgroundSubmitsFillAndBorderWithoutChangingControls)
{
	m_Ui.SetActiveItem(&m_Text);
	const void *pActive = m_Ui.ActiveItem();
	m_Ui.BeginBackgroundRender();
	DrawControls();
	m_Ui.EndBackgroundRender();
	ASSERT_EQ(m_Recording.m_vSurfaces.size(), 9u);
	EXPECT_GT(m_Recording.m_vSurfaces.front().m_FillColor.a, 0.0f);
	EXPECT_GT(m_Recording.m_vSurfaces[1].m_BorderColor.a, 0.0f);
	EXPECT_EQ(m_Ui.ActiveItem(), pActive);
	ExpectUnchanged();
}

TEST_F(QmBackgroundControlsIntegration, MeasurementSuppressesChromeAndNestedExitRestoresBackgroundDrawing)
{
	m_Ui.BeginBackgroundRender();
	m_Ui.BeginBackgroundRender();
	m_Ui.BeginRenderOnly();
	m_Ui.BeginRenderOnly();
	DrawControls();
	EXPECT_TRUE(m_Recording.m_vSurfaces.empty());
	m_Ui.EndRenderOnly();
	DrawControls();
	EXPECT_TRUE(m_Recording.m_vSurfaces.empty());
	m_Ui.EndRenderOnly();
	EXPECT_FALSE(m_Ui.MeasurementOnly());
	EXPECT_TRUE(m_Ui.RenderOnly());
	DrawControls();
	EXPECT_FALSE(m_Recording.m_vSurfaces.empty());
	m_Ui.EndBackgroundRender();
	EXPECT_TRUE(m_Ui.RenderOnly());
	m_Ui.EndBackgroundRender();
	EXPECT_FALSE(m_Ui.RenderOnly());
	ExpectUnchanged();
}

TEST_F(QmBackgroundControlsIntegration, BackgroundIgnoresPointerPressAndReleaseAndThenRestoresButtonInput)
{
	const vec2 Position = m_Rect.Center();
	const vec2 Pixels = Position * vec2(m_Recording.WindowWidth(), m_Recording.WindowHeight()) / m_Ui.Screen()->Size();
	m_Ui.OnCursorMove(Pixels.x, Pixels.y);
	const auto Frame = [&](bool Pressed, bool Background) {
		m_Input.m_HeldKey = Pressed ? KEY_MOUSE_1 : 0;
		m_Client.AdvanceFrame();
		m_Ui.Update();
		if(Background)
			m_Ui.BeginBackgroundRender();
		const bool Clicked = ui_widget::PrimaryButton(ui_widget::ControlContext(&m_Ui), &m_Button, "Connect", m_Rect);
		if(Background)
			m_Ui.EndBackgroundRender();
		return Clicked;
	};
	EXPECT_FALSE(Frame(false, true));
	EXPECT_FALSE(Frame(true, true));
	EXPECT_FALSE(Frame(false, true));
	EXPECT_EQ(m_Ui.ActiveItem(), nullptr);
	EXPECT_FALSE(Frame(false, false));
	EXPECT_FALSE(Frame(true, false));
	EXPECT_TRUE(Frame(false, false));
}

TEST_F(QmBackgroundControlsIntegration, BackgroundDoesNotCreateToggleAnimationState)
{
	CUiV2AnimationRuntime Animation;
	m_Ui.SetQmAnimationRuntime(&Animation);
	const auto Ctx = ui_widget::ControlContext(&m_Ui);
	const uint64_t KnobKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash ^ 0x5A5Aull, reinterpret_cast<uint64_t>(&m_Toggle));
	m_Ui.BeginBackgroundRender();
	ui_widget::DrawToggle(Ctx, &m_Toggle, true, m_Rect, true, true);
	m_Ui.EndBackgroundRender();
	EXPECT_FLOAT_EQ(Animation.GetValue(KnobKey, EUiAnimProperty::COLOR_MIX, -17.0f), -17.0f);
	m_Ui.BeginRenderOnly();
	ui_widget::DrawToggle(Ctx, &m_Toggle, true, m_Rect, true, true);
	m_Ui.EndRenderOnly();
	EXPECT_FLOAT_EQ(Animation.GetValue(KnobKey, EUiAnimProperty::COLOR_MIX, -17.0f), -17.0f);
	ui_widget::DrawToggle(Ctx, &m_Toggle, true, m_Rect, true, true);
	EXPECT_NE(Animation.GetValue(KnobKey, EUiAnimProperty::COLOR_MIX, -17.0f), -17.0f);
	m_Ui.SetQmAnimationRuntime(nullptr);
}

TEST_F(QmBackgroundControlsIntegration, PageBackgroundOpacityDoesNotMultiplyControlFillOrBorder)
{
	for(bool On : {false, true})
	{
		SCOPED_TRACE(On);
		m_Ui.SetBackgroundAlphaScale(1.0f);
		DrawOpacityControls(On);
		ASSERT_EQ(m_Recording.m_vSurfaces.size(), 4u);
		const auto Baseline = m_Recording.m_vSurfaces;
		EXPECT_FLOAT_EQ(Baseline[0].m_FillColor.a, 0.4f);
		EXPECT_FLOAT_EQ(Baseline[1].m_FillColor.a, 0.6f);
		if(!On)
			EXPECT_FLOAT_EQ(Baseline[2].m_FillColor.a, 0.4f);
		else
			EXPECT_GT(Baseline[2].m_FillColor.a, 0.0f);
		for(float PageAlpha : {0.0f, 0.2f, 0.6f, 1.0f})
		{
			SCOPED_TRACE(PageAlpha);
			m_Ui.SetBackgroundAlphaScale(PageAlpha);
			DrawOpacityControls(On);
			ASSERT_EQ(m_Recording.m_vSurfaces.size(), Baseline.size());
			for(size_t i = 0; i < Baseline.size(); ++i)
			{
				SCOPED_TRACE(i);
				EXPECT_EQ(m_Recording.m_vSurfaces[i].m_FillColor, Baseline[i].m_FillColor);
				EXPECT_EQ(m_Recording.m_vSurfaces[i].m_BorderColor, Baseline[i].m_BorderColor);
			}
		}
	}
}

TEST_F(QmBackgroundControlsIntegration, ParentTransitionFadesControlsExactlyOnceAndRestoresAfterScope)
{
	m_Ui.SetBackgroundAlphaScale(0.2f);
	DrawOpacityControls();
	ASSERT_EQ(m_Recording.m_vSurfaces.size(), 4u);
	const auto Baseline = m_Recording.m_vSurfaces;
	const auto Text = m_Ui.TextRender()->GetTextColor();
	const auto Outline = m_Ui.TextRender()->GetTextOutlineColor();
	const auto Selection = m_Ui.TextRender()->GetTextSelectionColor();
	for(float TransitionAlpha : {0.0f, 0.25f, 0.5f, 1.0f})
	{
		SCOPED_TRACE(TransitionAlpha);
		{
			CUiScopedControlAlphaScale Transition(&m_Ui, TransitionAlpha);
			DrawOpacityControls();
			EXPECT_FLOAT_EQ(m_Ui.TextRender()->GetTextColor().a, Text.a * TransitionAlpha);
			EXPECT_FLOAT_EQ(m_Ui.TextRender()->GetTextOutlineColor().a, Outline.a * TransitionAlpha);
			EXPECT_FLOAT_EQ(m_Ui.TextRender()->GetTextSelectionColor().a, Selection.a * TransitionAlpha);
			ASSERT_EQ(m_Recording.m_vSurfaces.size(), Baseline.size());
			for(size_t i = 0; i < Baseline.size(); ++i)
			{
				SCOPED_TRACE(i);
				EXPECT_FLOAT_EQ(m_Recording.m_vSurfaces[i].m_FillColor.a, Baseline[i].m_FillColor.a * TransitionAlpha);
				EXPECT_FLOAT_EQ(m_Recording.m_vSurfaces[i].m_BorderColor.a, Baseline[i].m_BorderColor.a * TransitionAlpha);
			}
		}
		EXPECT_EQ(m_Ui.TextRender()->GetTextColor(), Text);
		EXPECT_EQ(m_Ui.TextRender()->GetTextOutlineColor(), Outline);
		EXPECT_EQ(m_Ui.TextRender()->GetTextSelectionColor(), Selection);
		EXPECT_FLOAT_EQ(m_Ui.ControlAlphaScale(), 1.0f);
		EXPECT_FLOAT_EQ(m_Ui.BackgroundAlphaScale(), 0.2f);
		DrawOpacityControls();
		ASSERT_EQ(m_Recording.m_vSurfaces.size(), Baseline.size());
		for(size_t i = 0; i < Baseline.size(); ++i)
			EXPECT_EQ(m_Recording.m_vSurfaces[i].m_FillColor, Baseline[i].m_FillColor);
	}
}

TEST_F(QmBackgroundControlsIntegration, NestedParentTransitionsMultiplyEachLayerAndRestoreIndependently)
{
	m_Ui.SetBackgroundAlphaScale(0.2f);
	{
		CUiScopedControlAlphaScale Parent(&m_Ui, 0.5f);
		{
			CUiScopedControlAlphaScale Child(&m_Ui, 0.4f);
			DrawOpacityControls();
			ASSERT_EQ(m_Recording.m_vSurfaces.size(), 4u);
			EXPECT_NEAR(m_Recording.m_vSurfaces[0].m_FillColor.a, 0.4f * 0.5f * 0.4f, 0.0001f);
			EXPECT_NEAR(m_Recording.m_vSurfaces[1].m_FillColor.a, 0.6f * 0.5f * 0.4f, 0.0001f);
		}
		DrawOpacityControls();
		ASSERT_EQ(m_Recording.m_vSurfaces.size(), 4u);
		EXPECT_FLOAT_EQ(m_Recording.m_vSurfaces[0].m_FillColor.a, 0.4f * 0.5f);
	}
	EXPECT_FLOAT_EQ(m_Ui.ControlAlphaScale(), 1.0f);
}

TEST_F(QmBackgroundControlsIntegration, AnimatedPopupAppliesTransitionToItsControlsAndRestoresOutside)
{
	SPopupMenuId Id;
	struct SProbe
	{
		CUi *m_pUi;
		CButtonContainer m_Button;
		float m_Alpha = -1.0f;
	} Probe{&m_Ui};
	SPopupMenuProperties Props;
	Props.m_Animate = true;
	Props.m_BlockUnderlyingPointerInput = false;
	g_Config.m_QmUiPopupBlur = 0;
	g_Config.m_QmUiDropdownOpacity = 40;
	m_Ui.SetBackgroundAlphaScale(0.2f);
	m_Ui.DoPopupMenu(&Id, 20, 20, 240, 100, &Probe, [](void *pContext, CUIRect View, bool) {
		auto &State = *static_cast<SProbe *>(pContext);
		State.m_Alpha = State.m_pUi->ControlAlphaScale();
		ui_widget::DrawButtonSurface(ui_widget::ControlContext(State.m_pUi), &State.m_Button, View, {});
		return CUi::POPUP_KEEP_OPEN; }, Props);
	m_Ui.RenderPopupMenus();
	EXPECT_FLOAT_EQ(Probe.m_Alpha, 0.0f);
	EXPECT_FLOAT_EQ(m_Ui.ControlAlphaScale(), 1.0f);
	m_Client.AdvanceFrame();
	m_Recording.m_vSurfaces.clear();
	m_Ui.RenderPopupMenus();
	EXPECT_GT(Probe.m_Alpha, 0.0f);
	EXPECT_LT(Probe.m_Alpha, 1.0f);
	ASSERT_FALSE(m_Recording.m_vSurfaces.empty());
	EXPECT_NEAR(m_Recording.m_vSurfaces.back().m_FillColor.a, 0.4f * Probe.m_Alpha, 0.0001f);
	EXPECT_FLOAT_EQ(m_Ui.ControlAlphaScale(), 1.0f);
	for(int Frame = 0; Frame < 20; ++Frame)
		m_Client.AdvanceFrame();
	m_Ui.RenderPopupMenus();
	EXPECT_FLOAT_EQ(Probe.m_Alpha, 1.0f);
	m_Ui.ClosePopupMenus();
}

TEST_F(QmBackgroundControlsIntegration, SliderAccentFillAndHandleKeepOwnOpacityAcrossPageBackgrounds)
{
	m_Ui.BeginBackgroundRender();
	const auto Ctx = ui_widget::ControlContext(&m_Ui);
	m_Ui.SetBackgroundAlphaScale(1.0f);
	ui_widget::RenderHorizontalSlider(Ctx, &m_Button, m_Rect, 0.5f);
	ASSERT_EQ(m_Recording.m_vSurfaces.size(), 3u);
	const auto Baseline = m_Recording.m_vSurfaces;
	m_Recording.m_vSurfaces.clear();
	m_Ui.SetBackgroundAlphaScale(0.2f);
	ui_widget::RenderHorizontalSlider(Ctx, &m_Button, m_Rect, 0.5f);
	ASSERT_EQ(m_Recording.m_vSurfaces.size(), 3u);
	EXPECT_EQ(m_Recording.m_vSurfaces[1].m_FillColor, Baseline[1].m_FillColor);
	EXPECT_EQ(m_Recording.m_vSurfaces[2].m_FillColor, Baseline[2].m_FillColor);
	m_Ui.EndBackgroundRender();
}
