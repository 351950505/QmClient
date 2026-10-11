#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_BG_DRAW_STATE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_BG_DRAW_STATE_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/components/tclient/bg_draw_file.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <list>

class CQmBgDrawStroke
{
	CBgDrawItemData m_vData;
	bool m_Drawing = true;
	bool m_HasBounds = false;
	vec2 m_Min = vec2(0.0f, 0.0f);
	vec2 m_Max = vec2(0.0f, 0.0f);

	void ExtendBounds(const CBgDrawItemDataPoint &Point)
	{
		const vec2 Radius(Point.w, Point.w);
		const vec2 Min = Point.Pos() - Radius;
		const vec2 Max = Point.Pos() + Radius;
		m_Min = m_HasBounds ? vec2(std::min(m_Min.x, Min.x), std::min(m_Min.y, Min.y)) : Min;
		m_Max = m_HasBounds ? vec2(std::max(m_Max.x, Max.x), std::max(m_Max.y, Max.y)) : Max;
		m_HasBounds = true;
	}

public:
	static constexpr size_t MAX_POINTS = 1024;
	explicit CQmBgDrawStroke(const CBgDrawItemDataPoint &Point) :
		m_vData{Point}
	{
		ExtendBounds(Point);
	}
	explicit CQmBgDrawStroke(const CBgDrawItemData &Data) :
		m_vData(Data), m_Drawing(false)
	{
		for(const auto &Point : m_vData)
			ExtendBounds(Point);
	}
	const CBgDrawItemData &Data() const { return m_vData; }
	vec2 Min() const { return m_Min; }
	vec2 Max() const { return m_Max; }
	void PenUp() { m_Drawing = false; }
	bool MoveTo(const CBgDrawItemDataPoint &Point)
	{
		if(!m_Drawing)
			return false;
		const auto &Last = m_vData.back();
		if(Point.x == Last.x && Point.y == Last.y && Point.w == Last.w &&
			Point.r == Last.r && Point.g == Last.g && Point.b == Last.b && Point.a == Last.a)
			return false;
		if(m_vData.size() >= MAX_POINTS)
		{
			PenUp();
			return false;
		}
		if(m_vData.size() >= 2 && distance(m_vData[m_vData.size() - 2].Pos(), Last.Pos()) < Point.w * 0.75f)
			m_vData.back() = Point;
		else
			m_vData.push_back(Point);
		ExtendBounds(Point);
		return true;
	}
};

// 释放画笔借用后才能移除笔画，包括容量为零或整批替换的路径。
template<typename T, size_t N>
bool QmBgDrawMakeSpace(std::list<T> &Items, std::array<T *, N> &apActiveItems, int Capacity, int Count)
{
	if(Capacity <= 0 || Count >= Capacity)
	{
		apActiveItems.fill(nullptr);
		const bool Removed = !Items.empty();
		Items.clear();
		return Removed;
	}
	bool Removed = false;
	while(static_cast<int>(Items.size()) + Count > Capacity)
	{
		for(T *&pActiveItem : apActiveItems)
			if(pActiveItem == &Items.front())
				pActiveItem = nullptr;
		Items.pop_front();
		Removed = true;
	}
	return Removed;
}

#endif
