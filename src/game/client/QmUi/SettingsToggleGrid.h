#ifndef GAME_CLIENT_QMUI_SETTINGSTOGGLEGRID_H
#define GAME_CLIENT_QMUI_SETTINGSTOGGLEGRID_H

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

struct SSettingsToggleSize
{
	float m_Width = 0.0f;
	float m_Height = 0.0f;
};

template<typename TDimensions>
struct SSettingsToggleGridLayout
{
	int m_Columns = 1;
	int m_Rows = 0;
	float m_Gap = 0.0f;
	TDimensions m_aColumnWidths{};
	TDimensions m_aRowHeights{};

	template<typename TItems>
	void SetItemHeights(const TItems &aItems)
	{
		std::fill(m_aRowHeights.begin(), m_aRowHeights.end(), 0.0f);
		for(size_t Index = 0; Index < aItems.size(); ++Index)
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
using SSettingsToggleGrid = SSettingsToggleGridLayout<std::array<float, ItemCount>>;
using SSettingsDynamicToggleGrid = SSettingsToggleGridLayout<std::vector<float>>;

template<typename TItems, typename TDimensions>
void ResolveSettingsToggleGridLayout(float Width, const TItems &aItems, float Gap, SSettingsToggleGridLayout<TDimensions> &Grid)
{
	Width = std::max(0.0f, Width);
	Grid.m_Columns = 1;
	Grid.m_Rows = 0;
	Grid.m_Gap = std::max(0.0f, Gap);
	std::fill(Grid.m_aColumnWidths.begin(), Grid.m_aColumnWidths.end(), 0.0f);
	std::fill(Grid.m_aRowHeights.begin(), Grid.m_aRowHeights.end(), 0.0f);
	const int ItemCount = static_cast<int>(aItems.size());
	if(ItemCount == 0)
		return;

	const auto MeasureColumns = [&](int Columns) {
		// 每列只预留本列最长项的宽度，换列后必须重新核对实际分配。
		std::fill(Grid.m_aColumnWidths.begin(), Grid.m_aColumnWidths.end(), 0.0f);
		for(int Index = 0; Index < ItemCount; ++Index)
			Grid.m_aColumnWidths[Index % Columns] = std::max(Grid.m_aColumnWidths[Index % Columns], aItems[Index].m_Width);
		float RequiredWidth = (Columns - 1) * Grid.m_Gap;
		for(int Column = 0; Column < Columns; ++Column)
			RequiredWidth += Grid.m_aColumnWidths[Column];
		return RequiredWidth;
	};

	int BalancedColumns = 0;
	int MinimumRows = 0;
	for(int Columns = ItemCount; Columns >= 1; --Columns)
	{
		const int Rows = (ItemCount + Columns - 1) / Columns;
		if(BalancedColumns != 0 && Rows > MinimumRows)
			break;
		const float RequiredWidth = MeasureColumns(Columns);
		if(RequiredWidth > Width && Columns > 1)
			continue;

		// 不增加行数时优先减少列数，让 4+1、3+1 分别排成 3+2、2+2。
		BalancedColumns = Columns;
		MinimumRows = Rows;
	}
	Grid.m_Columns = BalancedColumns;
	Grid.m_Rows = MinimumRows;
	const float RequiredWidth = MeasureColumns(Grid.m_Columns);
	const float ExtraWidth = std::max(0.0f, Width - RequiredWidth) / Grid.m_Columns;
	float UsedWidth = 0.0f;
	for(int Column = 0; Column < Grid.m_Columns; ++Column)
	{
		Grid.m_aColumnWidths[Column] = Column + 1 == Grid.m_Columns ? std::max(0.0f, Width - UsedWidth) : Grid.m_aColumnWidths[Column] + ExtraWidth;
		UsedWidth += Grid.m_aColumnWidths[Column] + Grid.m_Gap;
	}
	Grid.SetItemHeights(aItems);
}

template<size_t ItemCount>
SSettingsToggleGrid<ItemCount> ResolveSettingsToggleGrid(float Width, const std::array<SSettingsToggleSize, ItemCount> &aItems, float Gap)
{
	SSettingsToggleGrid<ItemCount> Grid;
	ResolveSettingsToggleGridLayout(Width, aItems, Gap, Grid);
	return Grid;
}

// 动态分组复用菜单持有的容量，避免每张卡片在每帧重新分配尺寸数组。
inline void ResolveSettingsToggleGrid(float Width, const std::vector<SSettingsToggleSize> &vItems, float Gap, SSettingsDynamicToggleGrid &Grid)
{
	Grid.m_aColumnWidths.resize(vItems.size());
	Grid.m_aRowHeights.resize(vItems.size());
	ResolveSettingsToggleGridLayout(Width, vItems, Gap, Grid);
}

enum class ESettingsToggleGroupPass
{
	RENDER,
	LAYOUT,
	INPUT
};

// 同组短开关的数据不拥有配置或文案；调用时提供当帧有效的地址与译文。
struct SSettingsToggleEntry
{
	int *m_pValue;
	const char *m_pTextId;
	const char *m_pLabel;
	const void *m_pId = nullptr;
	int m_Mask = 0;
};

inline bool SettingsToggleEntryValue(const SSettingsToggleEntry &Entry)
{
	return Entry.m_Mask != 0 ? (*Entry.m_pValue & Entry.m_Mask) != 0 : *Entry.m_pValue != 0;
}

inline bool ApplySettingsToggleEntry(const SSettingsToggleEntry &Entry, bool Clicked, bool ProcessInput, bool Overridden)
{
	if(!Clicked || !ProcessInput || Overridden)
		return false;
	if(Entry.m_Mask != 0)
		*Entry.m_pValue ^= Entry.m_Mask;
	else
		*Entry.m_pValue ^= 1;
	return true;
}

inline SSettingsToggleSize ResolveSettingsToggleButtonSize(float LabelWidth, float LabelHeight, float MinimumHeight, float HorizontalPadding, float VerticalPadding)
{
	return {
		std::max(0.0f, LabelWidth) + std::max(0.0f, HorizontalPadding) * 2.0f,
		std::max(std::max(0.0f, MinimumHeight), std::max(0.0f, LabelHeight) + std::max(0.0f, VerticalPadding) * 2.0f)};
}

// 文本按钮的内边距同时用于译文测量和绘制，窄按钮不能产生负的正文区域。
inline CUIRect ResolveSettingsToggleButtonLabel(const CUIRect &Button, float HorizontalPadding, float VerticalPadding)
{
	const float Width = std::max(0.0f, Button.w);
	const float Height = std::max(0.0f, Button.h);
	const float PaddingX = std::clamp(HorizontalPadding, 0.0f, Width * 0.5f);
	const float PaddingY = std::clamp(VerticalPadding, 0.0f, Height * 0.5f);
	return {Button.x + PaddingX, Button.y + PaddingY, Width - PaddingX * 2.0f, Height - PaddingY * 2.0f};
}

#endif
