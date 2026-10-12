#include "UiDiscreteSlider.h"

#include "UiMotion.h"
#include "UiSurface.h"
#include "UiSurfaceText.h"
#include "UiTheme.h"

#include <engine/shared/config.h>

#include <game/client/ui.h>

namespace ui_widget
{
	namespace
	{
		void DrawSliderFill(const IUiContext &Ctx, const CUIRect &Fill, const SDiscreteSliderStyle &Style, float Alpha)
		{
			const float Radius = Fill.h * 0.5f;
			if(Style.m_Gradient && Fill.w > Fill.h)
			{
				// 两半共用中间色，各自只保留外侧圆角，不再叠画独立圆帽。
				CUIRect Left, Right;
				Fill.VSplitMid(&Left, &Right);
				Left.Draw4(Style.m_GradientStart.WithAlpha(Alpha), Style.m_GradientMiddle.WithAlpha(Alpha), Style.m_GradientStart.WithAlpha(Alpha), Style.m_GradientMiddle.WithAlpha(Alpha), IGraphics::CORNER_L, Radius);
				Right.Draw4(Style.m_GradientMiddle.WithAlpha(Alpha), Style.m_GradientEnd.WithAlpha(Alpha), Style.m_GradientMiddle.WithAlpha(Alpha), Style.m_GradientEnd.WithAlpha(Alpha), IGraphics::CORNER_R, Radius);
			}
			else
				DrawRoundedSurface(Ctx, Fill, Style.m_Color.WithAlpha(Alpha), ColorRGBA(), Radius);

			const float Time = Ctx.m_pAnim != nullptr && g_Config.m_QmUiMotionLevel != 0 ? Ctx.m_pAnim->TimeSec() : 0.0f;
			for(int Index = 0; Index < Style.m_ParticleCount; ++Index)
			{
				const SDiscreteSliderParticle Particle = ResolveDiscreteSliderParticle(Fill, Index, Time);
				if(Particle.m_Alpha <= 0.0f)
					continue;
				DrawRoundedSurface(Ctx, Particle.m_Rect, ui_token::color::TEXT_PRIMARY.WithAlpha(Particle.m_Alpha * Alpha), ColorRGBA(), Particle.m_Rect.w * 0.5f);
			}
		}
	}

	bool DiscreteSlider(const IUiContext &Ctx, const void *pId, SDiscreteSliderState &State, int *pValue, int Min, int Max, const CUIRect &Rect)
	{
		if(Ctx.m_pUi == nullptr || pValue == nullptr || pId == nullptr)
			return false;

		CUi *pUi = Ctx.m_pUi;
		pUi->DoConfigTooltip(pId, &Rect, pValue);
		const SDiscreteSliderGeometry Geometry = ResolveDiscreteSliderGeometry(Rect, Ctx.m_UiScale);
		if(!Geometry.IsUsable() || Max <= Min)
		{
			if(!pUi->RenderOnly() && pUi->IsActiveItem(pId))
				pUi->SetActiveItem(nullptr);
			return false;
		}

		bool Changed = false;
		bool Active = false;
		bool Hovered = false;
		if(!pUi->RenderOnly())
		{
			SDiscreteSliderInput Input;
			Input.m_MouseX = pUi->MouseX();
			Input.m_Pressed = pUi->MouseButtonClicked(0);
			Input.m_Down = pUi->MouseButton(0);
			Input.m_Hovered = pUi->MouseHovered(&Rect);
			Input.m_Active = pUi->CheckActiveItem(pId);
			Input.m_CanActivate = pUi->ActiveItem() == nullptr;
			Input.m_Enabled = pUi->Enabled();
			const SDiscreteSliderResult Result = UpdateDiscreteSlider(State, Geometry, Input, *pValue, Min, Max);
			*pValue = Result.m_Value;
			Changed = Result.m_Changed;
			Active = Result.m_Active;
			Hovered = Input.m_Hovered && Input.m_Enabled;
			if(Active)
				pUi->SetActiveItem(pId);
			else if(pUi->IsActiveItem(pId))
				pUi->SetActiveItem(nullptr);
			if(Hovered && (Active || !Input.m_Down))
				pUi->SetHotItem(pId);
		}
		// 本控件未渲染时（页面切走、调用点提前返回）active item 由 CUi::FinishCheck 当帧清理，
		// 因此不需要在这里额外兜底；抓取偏移在任一非拖动帧都会归零。

		CUiScopedGaussianBlurSuppression GaussianBlurSuppression(pUi);
		// 控制台只接管输入，底下的筛选状态不是被禁用的配置。
		const float Alpha = pUi->Enabled() || pUi->BackgroundRender() ? 1.0f : 0.45f;
		// 槽底和旋钮沿用公共样式；档位填色继续表达数值语义。
		const SUiTheme &Theme = Ctx.m_pTheme != nullptr ? *Ctx.m_pTheme : pUi->QmControlTheme();
		const auto SharedStyle = ResolveUiSliderStyle(Theme, CUiScopedSurfaceText::CurrentSurface(), Hovered, Active);
		DrawRoundedSurface(Ctx, Geometry.m_Track, SharedStyle.m_Track.WithMultipliedAlpha(Alpha), ColorRGBA(), ui_token::radius::PILL);

		const bool HasSelection = *pValue >= Min && *pValue <= Max;
		const SDiscreteSliderStyle Style = ResolveDiscreteSliderStyle(*pValue, Min, Max);
		const float Normalized = DiscreteSliderNormalizedValue(*pValue, Min, Max);
		if(HasSelection && *pValue > Min)
		{
			const CUIRect Fill = Geometry.FillRect(Normalized);
			DrawSliderFill(Ctx, Fill, Style, Alpha);
		}

		// 档位圆点数量与量程同阶：正常量程逐档绘制，异常大的量程直接跳过，
		// 避免 O(量程) 次绘制，也避免 int 递增在 Max == INT_MAX 时溢出。
		constexpr int64_t MAX_DRAWN_STOPS = 64;
		const int64_t NumStops = static_cast<int64_t>(Max) - static_cast<int64_t>(Min) + 1;
		if(NumStops <= MAX_DRAWN_STOPS)
		{
			for(int64_t Index = 0; Index < NumStops; ++Index)
			{
				const int Stop = Min + static_cast<int>(Index);
				CUIRect Dot;
				Dot.w = Dot.h = Geometry.m_DotSize;
				Dot.x = Geometry.Position(DiscreteSliderNormalizedValue(Stop, Min, Max)) - Dot.w * 0.5f;
				Dot.y = Rect.y + (Rect.h - Dot.h) * 0.5f;
				const float DotAlpha = HasSelection && Stop <= *pValue ? 0.42f : 0.30f;
				DrawRoundedSurface(Ctx, Dot, ResolveUiSurfaceForeground(CUiScopedSurfaceText::CurrentSurface()).WithAlpha(DotAlpha * Alpha), ColorRGBA(), Dot.w * 0.5f);
			}
		}

		return Changed;
	}
}
