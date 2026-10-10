#ifndef GAME_CLIENT_QMUI_SETTINGSTOGGLEGRID_H
#define GAME_CLIENT_QMUI_SETTINGSTOGGLEGRID_H

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cstddef>

struct SSettingsToggleSize
{
	float m_Width = 0.0f;
	float m_Height = 0.0f;
};

template<size_t ItemCount>
struct SSettingsToggleGrid
{
	int m_Columns = 1;
	int m_Rows = 0;
	float m_Gap = 0.0f;
	std::array<float, ItemCount> m_aColumnWidths{};
	std::array<float, ItemCount> m_aRowHeights{};

	void SetItemHeights(const std::array<SSettingsToggleSize, ItemCount> &aItems)
	{
		m_aRowHeights.fill(0.0f);
		for(size_t Index = 0; Index < ItemCount; ++Index)
			m_aRowHeights[Index / m_Columns] = std::max(m_aRowHeights[Index / m_Columns], aItems[Index].m_Height);
	}

	float Height() const
	{
		float Height = std::max(0, m_Rows - 1) * m_Gap;
		for(int Row = 0; Row < m_Rows; ++Row)
			Height += m_aRowHeights[Row];
		return Height;
	}

	CUIRect Cell(const CUIRect &Area, int Index) const
	{
		const int Column = Index % m_Columns;
		const int Row = Index / m_Columns;
		CUIRect Cell{Area.x, Area.y, m_aColumnWidths[Column], m_aRowHeights[Row]};
		for(int Previous = 0; Previous < Column; ++Previous)
			Cell.x += m_aColumnWidths[Previous] + m_Gap;
		for(int Previous = 0; Previous < Row; ++Previous)
			Cell.y += m_aRowHeights[Previous] + m_Gap;
		return Cell;
	}
};

template<size_t ItemCount>
SSettingsToggleGrid<ItemCount> ResolveSettingsToggleGrid(float Width, const std::array<SSettingsToggleSize, ItemCount> &aItems, float Gap)
{
	SSettingsToggleGrid<ItemCount> Grid;
	Width = std::max(0.0f, Width);
	Grid.m_Gap = std::max(0.0f, Gap);
	if constexpr(ItemCount == 0)
		return Grid;

	// 每列只预留本列最长项的宽度，不能用全组最长项把所有列一起撑宽。
	for(int Columns = static_cast<int>(ItemCount); Columns >= 1; --Columns)
	{
		Grid.m_aColumnWidths.fill(0.0f);
		for(size_t Index = 0; Index < ItemCount; ++Index)
			Grid.m_aColumnWidths[Index % Columns] = std::max(Grid.m_aColumnWidths[Index % Columns], aItems[Index].m_Width);
		float RequiredWidth = (Columns - 1) * Grid.m_Gap;
		for(int Column = 0; Column < Columns; ++Column)
			RequiredWidth += Grid.m_aColumnWidths[Column];
		if(RequiredWidth > Width && Columns > 1)
			continue;

		Grid.m_Columns = Columns;
		Grid.m_Rows = (static_cast<int>(ItemCount) + Columns - 1) / Columns;
		const float ExtraWidth = std::max(0.0f, Width - RequiredWidth) / Columns;
		float UsedWidth = 0.0f;
		for(int Column = 0; Column < Columns; ++Column)
		{
			Grid.m_aColumnWidths[Column] = Column + 1 == Columns ? std::max(0.0f, Width - UsedWidth) : Grid.m_aColumnWidths[Column] + ExtraWidth;
			UsedWidth += Grid.m_aColumnWidths[Column] + Grid.m_Gap;
		}
		Grid.SetItemHeights(aItems);
		return Grid;
	}
	return Grid;
}

#endif
