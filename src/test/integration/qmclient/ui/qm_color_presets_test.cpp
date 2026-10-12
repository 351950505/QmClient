#include <game/client/ui.h>

#include <gtest/gtest.h>

TEST(QmColorPresets, ApplyingPresetUpdatesBoundColorAndPreservesAlpha)
{
	for(const unsigned Rgb : {0xFFFFFFu, 0x171717u, 0xF25F5Cu, 0xFFB347u, 0xF7D154u, 0x5DBB82u, 0x4A90E2u, 0xA879E8u})
	{
		SCOPED_TRACE(Rgb);
		unsigned BoundColor = 0;
		CUi::SColorPickerPopupContext Context{};
		Context.m_Alpha = true;
		Context.m_pHslaColor = &BoundColor;
		Context.m_RgbaColor = ColorRGBA(0.2f, 0.3f, 0.4f, 0.35f);
		Context.ApplyPreset(Rgb);
		EXPECT_EQ(Context.m_RgbaColor.PackAlphaLast(false), Rgb);
		EXPECT_FLOAT_EQ(Context.m_RgbaColor.a, 0.35f);
		EXPECT_FLOAT_EQ(Context.m_HslaColor.a, 0.35f);
		EXPECT_FLOAT_EQ(Context.m_HsvaColor.a, 0.35f);
		EXPECT_EQ(BoundColor, Context.m_HslaColor.Pack(true));
		EXPECT_EQ(Context.m_State, EEditState::ONE_GO);
	}
}

TEST(QmColorPresets, OpaqueFieldKeepsItsHiddenAlphaAndCustomColorMode)
{
	unsigned BoundColor = 0;
	CUi::SColorPickerPopupContext Context{};
	Context.m_Alpha = false;
	Context.m_ColorMode = CUi::SColorPickerPopupContext::MODE_RGBA;
	Context.m_pHslaColor = &BoundColor;
	Context.m_RgbaColor = ColorRGBA(0.2f, 0.3f, 0.4f, 0.6f);
	Context.ApplyPreset(0x4A90E2);
	EXPECT_FLOAT_EQ(Context.m_RgbaColor.a, 0.6f);
	EXPECT_EQ(Context.m_ColorMode, CUi::SColorPickerPopupContext::MODE_RGBA);
	EXPECT_EQ(BoundColor, Context.m_HslaColor.Pack(false));
	Context.ApplyPreset(0xF25F5C);
	EXPECT_FLOAT_EQ(Context.m_RgbaColor.a, 0.6f);
	EXPECT_EQ(Context.m_RgbaColor.PackAlphaLast(false), 0xF25F5Cu);
}

TEST(QmColorPresets, UnboundPreviewCanApplyAndReplacePresets)
{
	CUi::SColorPickerPopupContext Context{};
	Context.m_RgbaColor = ColorRGBA(0.1f, 0.1f, 0.1f, 1.0f);
	Context.ApplyPreset(0xFFFFFF);
	EXPECT_EQ(Context.m_RgbaColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	Context.ApplyPreset(0x171717);
	EXPECT_EQ(Context.m_RgbaColor.PackAlphaLast(false), 0x171717u);
}
