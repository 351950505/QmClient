#include "QmCardCatalogInternal.h"

#include <engine/shared/config.h>

#include <game/localization.h>

#include <utility>

namespace qm_card_catalog
{
	bool BuildWaterHammerCard(const SQmCardBuildContext &Ctx, const qm_module::EQmModuleId Id, SSettingsCardDefinition &Out)
	{
		if(Id != qm_module::EQmModuleId::WaterHammerHighlight)
			return false;

		CMenus *pMenus = Ctx.m_pMenus;
		const float LineHeight = Ctx.m_Metrics.m_LineHeight;
		const float LineSpacing = Ctx.m_Metrics.m_LineSpacing;

		MakeModuleCard(
			Ctx,
			Id,
			"qm:water_hammer",
			"Water hammer highlight",
			"Highlight teammates holding hammer in death or freeze areas",
			[pMenus, LineHeight, LineSpacing](CUIRect &Content) {
				QmCardRenderHook::RenderQmVisualCheckbox(pMenus, Content, LineHeight, LineSpacing, &g_Config.m_QmWaterHammerHighlight, "Highlight water hammering teammates", Localize("Highlight teammates hammering in death or freeze areas"), &g_Config.m_QmWaterHammerHighlight);
			},
			Out);
		UseDeclaredModuleContent(Ctx, Out);
		return true;
	}
} // namespace qm_card_catalog
