#include <game/client/QmUi/SettingsPageLayout.h>
#include <game/client/QmUi/SettingsCardGeometry.h>
#include <game/client/QmUi/cards/QmCardMeasureRevision.h>

#include <gtest/gtest.h>

TEST(SettingsTextScale, ClampsPercentWithoutChangingResponsiveWidthScale)
{
	const auto Baseline = ResolveSettingsContentMetrics(680.0f, 100);
	const auto Small = ResolveSettingsContentMetrics(680.0f, 0);
	const auto Large = ResolveSettingsContentMetrics(680.0f, 300);
	EXPECT_FLOAT_EQ(Small.m_TextScale, 0.75f);
	EXPECT_FLOAT_EQ(Large.m_TextScale, 1.5f);
	EXPECT_FLOAT_EQ(Small.m_UiScale, Baseline.m_UiScale);
	EXPECT_FLOAT_EQ(Large.m_UiScale, Baseline.m_UiScale);
	EXPECT_FLOAT_EQ(Large.m_LabelWidth, Baseline.m_LabelWidth);
}

TEST(SettingsTextScale, TextAndInteractiveRowsGrowTogetherAtNarrowAndWideWidths)
{
	for(const float Width : {320.0f, 680.0f, 1000.0f})
		for(const int Percent : {75, 125, 150})
		{
			SCOPED_TRACE(Width);
			SCOPED_TRACE(Percent);
			const auto Baseline = ResolveSettingsContentMetrics(Width, 100);
			const auto Scaled = ResolveSettingsContentMetrics(Width, Percent);
			const float Factor = Percent / 100.0f;
			EXPECT_FLOAT_EQ(Scaled.m_BodySize, Baseline.m_BodySize * Factor);
			EXPECT_FLOAT_EQ(Scaled.m_SmallSize, Baseline.m_SmallSize * Factor);
			EXPECT_FLOAT_EQ(Scaled.m_HeadlineSize, Baseline.m_HeadlineSize * Factor);
			EXPECT_FLOAT_EQ(Scaled.m_LineHeight, Baseline.m_LineHeight * Factor);
			EXPECT_FLOAT_EQ(Scaled.m_InputHeight, Scaled.m_LineHeight);
			EXPECT_FLOAT_EQ(Scaled.m_ButtonHeight, Scaled.m_LineHeight);
			EXPECT_FLOAT_EQ(Scaled.m_ListRowHeight, Scaled.m_RowStep);
			EXPECT_FLOAT_EQ(ResolveSettingsRadioButtonHeight(Scaled), ResolveSettingsRadioButtonHeight(Baseline) * Factor);
			EXPECT_GT(Scaled.m_InputHeight, Scaled.m_BodySize);
		}
}

TEST(SettingsTextScale, PageDefinitionRevisionChangesWithEffectiveTextScale)
{
	const uint64_t Baseline = ResolveSettingsCardDefinitionsRevision(1, 2, 800.0f, 3, 100);
	EXPECT_NE(ResolveSettingsCardDefinitionsRevision(1, 2, 800.0f, 3, 150), Baseline);
	EXPECT_EQ(ResolveSettingsCardDefinitionsRevision(1, 2, 800.0f, 3, 300), ResolveSettingsCardDefinitionsRevision(1, 2, 800.0f, 3, 150));
	EXPECT_EQ(ResolveSettingsCardDefinitionsRevision(1, 2, 800.0f, 3, 100), Baseline);
}

TEST(SettingsTextScale, CardHeaderGeometryGrowsWithItsTitleAndActionHitboxes)
{
	const SSettingsCardSpec Spec{"scale", "Title", "Description"};
	const CUIRect Slot{10.0f, 20.0f, 400.0f, 0.0f};
	const auto Baseline = BuildSettingsCardFrame(Slot, Spec, 100.0f, 0.85f);
	const auto Large = BuildSettingsCardFrame(Slot, Spec, 150.0f, 0.85f, 1.5f);
	EXPECT_FLOAT_EQ(Large.m_TitleRect.h, Baseline.m_TitleRect.h * 1.5f);
	EXPECT_FLOAT_EQ(Large.m_HeaderRect.h, Baseline.m_HeaderRect.h * 1.5f);
	EXPECT_FLOAT_EQ(Large.m_HandleRect.h, Baseline.m_HandleRect.h * 1.5f);
	EXPECT_FLOAT_EQ(Large.m_ContentRect.h, 150.0f);
	EXPECT_GT(Large.m_ContentRect.y, Baseline.m_ContentRect.y);
	EXPECT_LE(Large.m_TitleRect.x + Large.m_TitleRect.w, Large.m_HandleRect.x);
	EXPECT_LE(Large.m_HandleRect.y + Large.m_HandleRect.h, Large.m_HeaderRect.y + Large.m_HeaderRect.h);
	EXPECT_FLOAT_EQ(Large.DisplayRect().h, Large.HitRect().h);
}

namespace
{
	class SettingsConfiguredTextScale : public testing::Test
	{
	protected:
		CConfig m_Previous = g_Config;
		void TearDown() override { g_Config = m_Previous; }
	};
}

TEST_F(SettingsConfiguredTextScale, DefaultMetricsAndModuleRevisionTrackTheSameSetting)
{
	g_Config.m_QmUiTextScale = 100;
	const auto Baseline = ResolveSettingsContentMetrics(800.0f);
	const uint64_t ModuleRevision = qm_card_catalog::MeasureModuleCardRevision(qm_module::EQmModuleId::Tooltip);
	g_Config.m_QmUiTextScale = 150;
	EXPECT_GT(ResolveSettingsContentMetrics(800.0f).m_InputHeight, Baseline.m_InputHeight);
	EXPECT_NE(qm_card_catalog::MeasureModuleCardRevision(qm_module::EQmModuleId::Tooltip), ModuleRevision);
	g_Config.m_QmUiTextScale = 100;
	EXPECT_EQ(qm_card_catalog::MeasureModuleCardRevision(qm_module::EQmModuleId::Tooltip), ModuleRevision);
}
