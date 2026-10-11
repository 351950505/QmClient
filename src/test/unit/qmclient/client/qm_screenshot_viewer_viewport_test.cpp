#include <game/client/components/qmclient/screenshot_viewer_viewport.h>

#include <gtest/gtest.h>

#include <limits>

TEST(QmScreenshotViewerViewport, FitsLargeImageWithoutChangingAspectRatio)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1920, 1080);
	const auto Rect = Viewport.ImageRect();
	EXPECT_FLOAT_EQ(Rect.m_Width, 800.0f);
	EXPECT_FLOAT_EQ(Rect.m_Height, 450.0f);
	EXPECT_FLOAT_EQ(Rect.m_X, 0.0f);
	EXPECT_FLOAT_EQ(Rect.m_Y, 75.0f);
}

TEST(QmScreenshotViewerViewport, CentersSmallImageAtOriginalSize)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(200, 100);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 1.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 300.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 250.0f);
}

TEST(QmScreenshotViewerViewport, FitModeFollowsWindowResize)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.Resize(400, 600);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 0.25f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Height, 300.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 150.0f);
}

TEST(QmScreenshotViewerViewport, UnchangedZoomPreservesFitOnResize)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ZoomAt(1.0f, 400.0f, 300.0f);
	Viewport.Resize(400, 300);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 0.25f);
}

TEST(QmScreenshotViewerViewport, ToggleReturnsFromOriginalSizeToFit)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ToggleFit();
	EXPECT_FLOAT_EQ(Viewport.Scale(), 1.0f);
	Viewport.Pan(100.0f, 50.0f);
	Viewport.ToggleFit();
	EXPECT_FLOAT_EQ(Viewport.Scale(), 0.5f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 0.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 0.0f);
}

TEST(QmScreenshotViewerViewport, ZoomKeepsImagePointUnderCursor)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ZoomAt(2.0f, 200.0f, 150.0f);
	const auto Rect = Viewport.ImageRect();
	// 缩放前鼠标位于图片的 (400, 300) 像素，该像素缩放后仍在鼠标下。
	EXPECT_FLOAT_EQ(Rect.m_X + 400.0f * Viewport.Scale(), 200.0f);
	EXPECT_FLOAT_EQ(Rect.m_Y + 300.0f * Viewport.Scale(), 150.0f);
}

TEST(QmScreenshotViewerViewport, PanCannotMoveImageBeyondViewportEdges)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ActualSize();
	Viewport.Pan(10000.0f, 10000.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 0.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 0.0f);
	Viewport.Pan(-10000.0f, -10000.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X + Viewport.ImageRect().m_Width, 800.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y + Viewport.ImageRect().m_Height, 600.0f);
}

TEST(QmScreenshotViewerViewport, PanKeepsFittedImageCentered)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 800);
	Viewport.Pan(500.0f, -500.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 0.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 100.0f);
}

TEST(QmScreenshotViewerViewport, ResizePreservesManualZoomAndRecentersSmallImage)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ActualSize();
	Viewport.Pan(100.0f, 100.0f);
	Viewport.Resize(2000, 1600);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 1.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 200.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 200.0f);
}

TEST(QmScreenshotViewerViewport, ZoomHasFiniteUpperAndLowerBounds)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ZoomAt(1e30f, 400.0f, 300.0f);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 32.0f);
	Viewport.ZoomAt(1e-30f, 400.0f, 300.0f);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 0.01f);
}

TEST(QmScreenshotViewerViewport, InvalidGeometryAndZoomDoNotProduceInvalidRect)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(0, -1);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ZoomAt(2.0f, 0.0f, 0.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Width, 0.0f);
	Viewport.Resize(800, 600);
	Viewport.ZoomAt(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
	Viewport.ZoomAt(-1.0f, 0.0f, 0.0f);
	Viewport.ZoomAt(0.0f, 0.0f, 0.0f);
	Viewport.Pan(std::numeric_limits<float>::infinity(), 0.0f);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 0.5f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 0.0f);
}

TEST(QmScreenshotViewerViewport, ReplacingImageResetsZoomAndPan)
{
	CQmScreenshotViewerViewport Viewport;
	Viewport.Resize(800, 600);
	Viewport.SetImageSize(1600, 1200);
	Viewport.ActualSize();
	Viewport.Pan(200.0f, 100.0f);
	Viewport.SetImageSize(400, 200);
	EXPECT_FLOAT_EQ(Viewport.Scale(), 1.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_X, 200.0f);
	EXPECT_FLOAT_EQ(Viewport.ImageRect().m_Y, 200.0f);
}
