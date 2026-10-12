#include <game/client/components/qmclient/console_appearance.h>

#include <gtest/gtest.h>

TEST(QmConsoleAppearance, DefaultTerminalUsesDarkBackgroundAndLightText)
{
	const auto Palette = QmConsoleAppearance::ResolvePalette(0, 96, {});
	EXPECT_LT(Palette.m_aColors[QmConsoleAppearance::BACKGROUND].r, 0.1f);
	EXPECT_GT(Palette.m_aColors[QmConsoleAppearance::TEXT].r, 0.75f);
	EXPECT_FLOAT_EQ(Palette.m_aColors[QmConsoleAppearance::BACKGROUND].a, 0.96f);
	EXPECT_FLOAT_EQ(Palette.m_aColors[QmConsoleAppearance::TEXT].a, 1.0f);
}

TEST(QmConsoleAppearance, ClassicPresetUsesABlueBackground)
{
	const auto Palette = QmConsoleAppearance::ResolvePalette(1, 100, {});
	const auto Background = Palette.m_aColors[QmConsoleAppearance::BACKGROUND];
	EXPECT_GT(Background.b, Background.g);
	EXPECT_GT(Background.g, Background.r);
}

TEST(QmConsoleAppearance, CustomColorsKeepOpaqueTextAndIndependentBackgroundOpacity)
{
	std::array<unsigned, QmConsoleAppearance::COLOR_COUNT> aCustom{};
	aCustom.fill(ColorHSLA(0.5f, 0.75f, 0.6f, 1.0f).Pack(false));
	const auto Palette = QmConsoleAppearance::ResolvePalette(2, 50, aCustom);
	const auto Expected = color_cast<ColorRGBA>(ColorHSLA(aCustom[0]));
	EXPECT_EQ(Palette.m_aColors[QmConsoleAppearance::TEXT], Expected);
	EXPECT_FLOAT_EQ(Palette.m_aColors[QmConsoleAppearance::BACKGROUND].r, Expected.r);
	EXPECT_FLOAT_EQ(Palette.m_aColors[QmConsoleAppearance::BACKGROUND].a, 0.5f);
}

TEST(QmConsoleAppearance, InvalidExternalValuesHaveBoundedFallbacks)
{
	EXPECT_EQ(QmConsoleAppearance::ResolvePalette(99, 96, {}).m_aColors, QmConsoleAppearance::ResolvePalette(0, 96, {}).m_aColors);
	EXPECT_FLOAT_EQ(QmConsoleAppearance::ResolvePalette(0, -10, {}).m_aColors[0].a, 0.2f);
	EXPECT_FLOAT_EQ(QmConsoleAppearance::ResolvePalette(0, 200, {}).m_aColors[0].a, 1.0f);
	EXPECT_FLOAT_EQ(QmConsoleAppearance::FontSize(-1), 1.0f);
	EXPECT_FLOAT_EQ(QmConsoleAppearance::FontSize(100), 24.0f);
}

TEST(QmConsoleAppearance, ResetRestoresAppearanceAndPreservesLogFilters)
{
	CConfig Config{};
	Config.m_QmConsoleFilterMask = 2;
	Config.m_QmConsoleFontSize = 24;
	Config.m_QmConsoleColorScheme = 2;
	Config.m_QmConsoleOpacity = 20;
	Config.m_QmConsoleHighlightCommands = 0;
	QmConsoleAppearance::Reset(Config);
	EXPECT_EQ(Config.m_QmConsoleFilterMask, 2);
	EXPECT_EQ(Config.m_QmConsoleFontSize, 10);
	EXPECT_EQ(Config.m_QmConsoleColorScheme, 0);
	EXPECT_EQ(Config.m_QmConsoleOpacity, 96);
	EXPECT_EQ(Config.m_QmConsoleHighlightCommands, 1);
	EXPECT_EQ(Config.m_QmConsoleLinkColor, DefaultConfig::QmConsoleLinkColor);
	EXPECT_EQ(Config.m_QmConsoleSearchSelectedColor, DefaultConfig::QmConsoleSearchSelectedColor);
}

TEST(QmConsoleAppearance, FontSizeAllowsOneThroughSevenWithoutClampingToEight)
{
	for(int Size = 1; Size <= 7; ++Size)
		EXPECT_FLOAT_EQ(QmConsoleAppearance::FontSize(Size), static_cast<float>(Size));
}

TEST(QmConsoleAppearance, PowerShellSevenPresetDistinguishesCommandParameterAndString)
{
	const auto Palette = QmConsoleAppearance::ResolvePalette(3, 40, {});
	const auto Command = Palette.m_aColors[QmConsoleAppearance::COMMAND];
	const auto Parameter = Palette.m_aColors[QmConsoleAppearance::PARAMETER];
	const auto String = Palette.m_aColors[QmConsoleAppearance::STRING];
	EXPECT_GT(Command.r, Command.b);
	EXPECT_GT(Parameter.b, Parameter.r);
	EXPECT_GT(String.g, String.r);
	EXPECT_FLOAT_EQ(Palette.m_aColors[QmConsoleAppearance::BACKGROUND].a, 0.4f);
	EXPECT_FLOAT_EQ(Command.a, 1.0f);
	EXPECT_FLOAT_EQ(Parameter.a, 1.0f);
	EXPECT_FLOAT_EQ(String.a, 1.0f);
}

TEST(QmConsoleAppearance, SwitchingPresetKeepsTheConfiguredCustomPalette)
{
	CConfig Config{};
	QmConsoleAppearance::Reset(Config);
	Config.m_QmConsoleCommandColor = ColorHSLA(0.7f, 0.8f, 0.6f).Pack(false);
	Config.m_QmConsoleColorScheme = 2;
	const auto Custom = QmConsoleAppearance::Palette(Config);
	Config.m_QmConsoleColorScheme = 3;
	const auto Preset = QmConsoleAppearance::Palette(Config);
	EXPECT_NE(Preset.m_aColors[QmConsoleAppearance::COMMAND], Custom.m_aColors[QmConsoleAppearance::COMMAND]);
	Config.m_QmConsoleColorScheme = 2;
	EXPECT_EQ(QmConsoleAppearance::Palette(Config).m_aColors, Custom.m_aColors);
}
