#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_BINDWHEEL_STATE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_BINDWHEEL_STATE_H

#include <base/vmath.h>

#include <cmath>
#include <optional>
#include <utility>

class CQmBindWheelState
{
	bool m_Active = false;
	std::optional<int> m_PendingBind;

public:
	bool IsActive() const { return m_Active; }
	void Open()
	{
		m_Active = true;
		m_PendingBind.reset();
	}
	void Release(int SelectedBind)
	{
		if(m_Active)
			m_PendingBind = SelectedBind;
		m_Active = false;
	}
	void Cancel()
	{
		m_Active = false;
		InvalidateSelection();
	}
	void InvalidateSelection() { m_PendingBind.reset(); }
	std::optional<int> TakePendingBind() { return std::exchange(m_PendingBind, std::nullopt); }

	static int SelectedBind(vec2 Mouse, int Count)
	{
		if(Count <= 0 || length(Mouse) <= 110.0f)
			return -1;
		const int Segment = static_cast<int>(std::round(angle(Mouse) / (2.0f * pi) * Count));
		return (Segment % Count + Count) % Count;
	}
};

#endif
