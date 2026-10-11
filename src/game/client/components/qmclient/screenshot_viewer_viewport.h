#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCREENSHOT_VIEWER_VIEWPORT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCREENSHOT_VIEWER_VIEWPORT_H

#include <algorithm>
#include <cmath>

// 独立看图窗口的视口；坐标使用渲染像素，100% 时一个图片像素对应一个屏幕像素。
class CQmScreenshotViewerViewport
{
public:
	struct SRect
	{
		float m_X;
		float m_Y;
		float m_Width;
		float m_Height;
	};

private:
	int m_ImageWidth = 0;
	int m_ImageHeight = 0;
	int m_Width = 0;
	int m_Height = 0;
	float m_Scale = 1.0f;
	float m_OffsetX = 0.0f;
	float m_OffsetY = 0.0f;
	bool m_Fit = true;

	bool Valid() const { return m_ImageWidth > 0 && m_ImageHeight > 0 && m_Width > 0 && m_Height > 0; }
	float FitScale() const
	{
		return Valid() ? std::min({1.0f, (float)m_Width / m_ImageWidth, (float)m_Height / m_ImageHeight}) : 1.0f;
	}
	void ClampOffsets()
	{
		const float MaxX = std::max(0.0f, (m_ImageWidth * m_Scale - m_Width) * 0.5f);
		const float MaxY = std::max(0.0f, (m_ImageHeight * m_Scale - m_Height) * 0.5f);
		m_OffsetX = std::clamp(m_OffsetX, -MaxX, MaxX);
		m_OffsetY = std::clamp(m_OffsetY, -MaxY, MaxY);
	}

public:
	void SetImageSize(int Width, int Height)
	{
		m_ImageWidth = std::max(0, Width);
		m_ImageHeight = std::max(0, Height);
		Fit();
	}
	void Resize(int Width, int Height)
	{
		m_Width = std::max(0, Width);
		m_Height = std::max(0, Height);
		if(m_Fit)
			Fit();
		else
			ClampOffsets();
	}
	void Fit()
	{
		m_Fit = true;
		m_Scale = FitScale();
		m_OffsetX = m_OffsetY = 0.0f;
	}
	void ActualSize()
	{
		m_Fit = false;
		m_Scale = 1.0f;
		m_OffsetX = m_OffsetY = 0.0f;
	}
	void ToggleFit()
	{
		if(m_Fit)
			ActualSize();
		else
			Fit();
	}
	void ZoomAt(float Factor, float X, float Y)
	{
		if(!Valid() || !std::isfinite(Factor) || Factor <= 0.0f || !std::isfinite(X) || !std::isfinite(Y))
			return;
		const float Scale = std::clamp(m_Scale * Factor, std::min(0.01f, FitScale()), 32.0f);
		if(Scale == m_Scale)
			return;
		const float Ratio = Scale / m_Scale;
		m_OffsetX = (X - m_Width * 0.5f) * (1.0f - Ratio) + m_OffsetX * Ratio;
		m_OffsetY = (Y - m_Height * 0.5f) * (1.0f - Ratio) + m_OffsetY * Ratio;
		m_Scale = Scale;
		m_Fit = false;
		ClampOffsets();
	}
	void Pan(float X, float Y)
	{
		if(!Valid() || !std::isfinite(X) || !std::isfinite(Y))
			return;
		m_OffsetX += X;
		m_OffsetY += Y;
		ClampOffsets();
	}
	float Scale() const { return m_Scale; }
	SRect ImageRect() const
	{
		if(!Valid())
			return {};
		const float Width = m_ImageWidth * m_Scale;
		const float Height = m_ImageHeight * m_Scale;
		return {(m_Width - Width) * 0.5f + m_OffsetX, (m_Height - Height) * 0.5f + m_OffsetY, Width, Height};
	}
};

#endif
