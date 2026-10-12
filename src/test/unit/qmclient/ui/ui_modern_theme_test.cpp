#include <game/client/QmUi/UiTheme.h>

#include <gtest/gtest.h>

TEST(UiModernTheme, SelectionUsesAccentAndIgnoresLegacySelectionColor)
{
	const ColorHSLA Accent(0.62f, 0.8f, 0.5f, 0.6f);
	const auto Theme = ResolveUiTheme(ColorHSLA(0), 1.0f, ColorHSLA(0), Accent, ColorHSLA(0.0f, 1.0f, 0.5f));
	EXPECT_FLOAT_EQ(Theme.m_Selected.r, Theme.m_Accent.r);
	EXPECT_FLOAT_EQ(Theme.m_Selected.g, Theme.m_Accent.g);
	EXPECT_FLOAT_EQ(Theme.m_Selected.b, Theme.m_Accent.b);
	EXPECT_FLOAT_EQ(Theme.m_Selected.a, 0.22f * Theme.m_Accent.a);
	EXPECT_EQ(Theme.m_Selected, ResolveUiTheme(ColorHSLA(0), 1.0f, ColorHSLA(0), Accent, ColorHSLA(0.3f, 1.0f, 0.5f)).m_Selected);
}

TEST(UiModernTheme, BackgroundOpacityDoesNotDimBordersAccentOrFocus)
{
	const ColorHSLA Accent(0.62f, 0.8f, 0.5f, 0.6f);
	const auto Opaque = ResolveUiTheme(ColorHSLA(0), 1.0f, ColorHSLA(0x97FFA6), Accent);
	const auto Transparent = ResolveUiTheme(ColorHSLA(0), 0.0f, ColorHSLA(0x97FFA6), Accent);
	EXPECT_FLOAT_EQ(Transparent.m_Surface.a, 0.0f);
	EXPECT_FLOAT_EQ(Transparent.m_InputSurface.a, 0.0f);
	EXPECT_EQ(Transparent.m_Border, Opaque.m_Border);
	EXPECT_EQ(Transparent.m_BorderHovered, Opaque.m_BorderHovered);
	EXPECT_EQ(Transparent.m_BorderFocused, Opaque.m_BorderFocused);
	EXPECT_EQ(Transparent.m_FocusRing, Opaque.m_FocusRing);
	EXPECT_EQ(Transparent.m_Accent, Opaque.m_Accent);
	EXPECT_EQ(Transparent.m_Selected, Opaque.m_Selected);
}

TEST(UiModernTheme, TransparentAccentAlsoMakesSelectionTransparent)
{
	const auto Theme = ResolveUiTheme(ColorHSLA(0), 1.0f, ColorHSLA(0), ColorHSLA(0.62f, 0.8f, 0.5f, 0.0f));
	EXPECT_FLOAT_EQ(Theme.m_Selected.a, 0.0f);
	EXPECT_FLOAT_EQ(Theme.m_BorderHovered.a, 0.0f);
	EXPECT_FLOAT_EQ(Theme.m_BorderFocused.a, 0.0f);
	EXPECT_GT(Theme.m_FocusRing.a, 0.0f);
}
