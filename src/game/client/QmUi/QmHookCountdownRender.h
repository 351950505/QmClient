#ifndef GAME_CLIENT_QMUI_QMHOOKCOUNTDOWNRENDER_H
#define GAME_CLIENT_QMUI_QMHOOKCOUNTDOWNRENDER_H

#include "QmPieMenuRender.h"

#include <game/client/components/qmclient/hook_countdown.h>

namespace qm_hook_countdown_ui
{
	constexpr float OUTER_RADIUS = 12.5f;
	constexpr float INNER_RADIUS = 8.5f;
	constexpr float INFINITY_OUTER_RADIUS = 18.0f;

	// 任意弧段：StartAngle/EndAngle 为弧度，0 = 环顶部，向顺时针增，EndAngle > StartAngle。
	// 新后端走 RenderProceduralRing（旋转+对称扇区参数化），老后端复用 DrawSector。
	inline void DrawArcSegment(IGraphics *pGraphics, vec2 Center, float InnerRadius, float OuterRadius, float StartAngle, float EndAngle, ColorRGBA Color, float PixelSize)
	{
		if(EndAngle <= StartAngle || Color.a <= 0.0f)
			return;
		if(pGraphics->HasProceduralRing())
		{
			// 绘制框留出羽化空间，不在圆环外沿截断抗锯齿。
			const float Extent = OuterRadius + PixelSize * 1.5f;
			IGraphics::SProceduralRingParams Params;
			Params.m_Rect = vec4(Center.x - Extent, Center.y - Extent, Extent * 2.0f, Extent * 2.0f);
			Params.m_Color = Color;
			Params.m_RingInnerRadius = InnerRadius / (Extent * 2.0f);
			Params.m_RingOuterRadius = OuterRadius / (Extent * 2.0f);
			// 扇区避开 atan 的正负 pi 接缝，再旋到段中心，避免羽化产生径向暗缝。
			const float SegmentCenter = (StartAngle + EndAngle) * 0.5f;
			const float HalfSweep = (EndAngle - StartAngle) * 0.5f;
			Params.m_RingStartAngle = -HalfSweep;
			Params.m_RingEndAngle = HalfSweep;
			Params.m_Rotation = -pi / 2.0f + SegmentCenter;
			pGraphics->RenderProceduralRing(Params);
		}
		else
		{
			// 老后端复用已有扇区几何与羽化；这里没有相邻选项，不限制端面的羽化宽度。
			constexpr float RadToDeg = 180.0f / pi;
			qm_pie_menu_ui::DrawSector(pGraphics, Center, InnerRadius, OuterRadius, -90.0f + StartAngle * RadToDeg, -90.0f + EndAngle * RadToDeg, 360.0f, Color);
		}
	}

	inline void DrawArc(IGraphics *pGraphics, vec2 Center, float InnerRadius, float OuterRadius, float Progress, ColorRGBA Color, float PixelSize)
	{
		if(Progress <= 0.0f || Color.a <= 0.0f)
			return;
		DrawArcSegment(pGraphics, Center, InnerRadius, OuterRadius, 0.0f, 2.0f * pi * Progress, Color, PixelSize);
	}

	inline void DrawInfinity(IGraphics *pGraphics, const SQmHookCountdownVisual &Visual, float PixelSize)
	{
		const auto &aPath = QmHookCountdownInfinityPath();
		const float Breath = QmHookCountdownBreath(Visual.m_BreathPhase);
		const float Scale = Visual.m_Scale * (0.97f + Breath * 0.05f);
		const float CoreHalfWidth = std::max(0.35f * Scale, PixelSize * 0.35f);
		const float aHalfWidths[] = {CoreHalfWidth, CoreHalfWidth + 0.5f * Scale, CoreHalfWidth + 1.45f * Scale, CoreHalfWidth + 2.85f * Scale, CoreHalfWidth + 5.05f * Scale + PixelSize};
		const float aAlphas[] = {0.76f, 0.46f, 0.14f, 0.02f, 0.0f};
		const float GlowAlpha = Visual.m_Alpha * (0.65f + Breath * 0.3f);
		std::array<ColorRGBA, 97> aColors;
		for(size_t i = 0; i < aPath.size(); ++i)
			aColors[i] = QmHookCountdownFlowColor(Visual.m_FlowPhase, aPath[i].m_PathPosition);

		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		// 两侧用连续透明度带形成外发光，曲线和颜色端点共享，避免旧圆环的分段暗缝。
		for(size_t Layer = 0; Layer + 1 < std::size(aHalfWidths); ++Layer)
		{
			for(size_t i = 0; i + 1 < aPath.size(); ++i)
			{
				const auto &From = aPath[i];
				const auto &To = aPath[i + 1];
				const IGraphics::CColorVertex aVertices[] = {
					{0, aColors[i].WithAlpha(GlowAlpha * aAlphas[Layer])},
					{1, aColors[i + 1].WithAlpha(GlowAlpha * aAlphas[Layer])},
					{2, aColors[i].WithAlpha(GlowAlpha * aAlphas[Layer + 1])},
					{3, aColors[i + 1].WithAlpha(GlowAlpha * aAlphas[Layer + 1])}};
				pGraphics->SetColorVertex(aVertices, std::size(aVertices));
				for(float Side : {-1.0f, 1.0f})
				{
					const IGraphics::CFreeformItem Quad(
						Visual.m_Position + From.m_Position * Scale + From.m_Normal * (aHalfWidths[Layer] * Side),
						Visual.m_Position + To.m_Position * Scale + To.m_Normal * (aHalfWidths[Layer] * Side),
						Visual.m_Position + From.m_Position * Scale + From.m_Normal * (aHalfWidths[Layer + 1] * Side),
						Visual.m_Position + To.m_Position * Scale + To.m_Normal * (aHalfWidths[Layer + 1] * Side));
					pGraphics->QuadsDrawFreeform(&Quad, 1);
				}
			}
		}
		// 浅色细芯最后绘制，保证交叉处不会被后绘制的彩色光晕盖住。
		pGraphics->SetColor(ColorRGBA(0.95f, 0.98f, 1.0f, Visual.m_Alpha * (0.72f + Breath * 0.14f)));
		for(size_t i = 0; i + 1 < aPath.size(); ++i)
		{
			const auto &From = aPath[i];
			const auto &To = aPath[i + 1];
			const IGraphics::CFreeformItem Quad(
				Visual.m_Position + From.m_Position * Scale - From.m_Normal * CoreHalfWidth,
				Visual.m_Position + To.m_Position * Scale - To.m_Normal * CoreHalfWidth,
				Visual.m_Position + From.m_Position * Scale + From.m_Normal * CoreHalfWidth,
				Visual.m_Position + To.m_Position * Scale + To.m_Normal * CoreHalfWidth);
			pGraphics->QuadsDrawFreeform(&Quad, 1);
		}
		pGraphics->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		pGraphics->QuadsEnd();
	}

	inline void Render(IGraphics *pGraphics, const SQmHookCountdownVisual &Visual, float PixelSize)
	{
		if(Visual.m_EndlessHook)
		{
			DrawInfinity(pGraphics, Visual, PixelSize);
			return;
		}
		const float InnerRadius = INNER_RADIUS * Visual.m_Scale;
		const float OuterRadius = OUTER_RADIUS * Visual.m_Scale;
		const float Outline = std::min(PixelSize * 0.7f, InnerRadius * 0.25f);
		DrawArc(pGraphics, Visual.m_Position, InnerRadius - Outline, OuterRadius + Outline, 1.0f, ColorRGBA(0.04f, 0.07f, 0.11f, Visual.m_Alpha * 0.45f), PixelSize);
		DrawArc(pGraphics, Visual.m_Position, InnerRadius, OuterRadius, 1.0f, Visual.m_Color.WithAlpha(Visual.m_Alpha * 0.20f), PixelSize);
		DrawArc(pGraphics, Visual.m_Position, InnerRadius, OuterRadius, Visual.m_Progress, Visual.m_Color.WithAlpha(Visual.m_Alpha), PixelSize);
	}
}

#endif
