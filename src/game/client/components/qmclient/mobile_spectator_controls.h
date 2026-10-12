#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MOBILE_SPECTATOR_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MOBILE_SPECTATOR_CONTROLS_H

#include <base/detect.h>
#include <base/vmath.h>

#include <engine/input.h>

#include <generated/protocol.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace qm_mobile_spectator
{
	constexpr bool Enabled()
	{
#if defined(CONF_PLATFORM_ANDROID)
		return true;
#else
		return false;
#endif
	}

	enum EAction
	{
		PREVIOUS,
		SELECT,
		NEXT,
		FREEVIEW,
		ZOOM_OUT,
		ZOOM_IN,
		MENU,
		CHAT,
		PAUSE,
		NUM_ACTIONS,
	};

	struct SLayout
	{
		CUIRect m_Joystick;
		std::array<CUIRect, NUM_ACTIONS> m_aButtons;
	};

	inline SLayout Layout(vec2 Screen)
	{
		SLayout Result{};
		Result.m_Joystick = {18.0f, Screen.y - 142.0f, 104.0f, 104.0f};
		for(int i = PREVIOUS; i <= ZOOM_IN; ++i)
			Result.m_aButtons[i] = {Screen.x - 180.0f + (i % 3) * 54.0f, Screen.y - 118.0f + (i / 3) * 44.0f, 48.0f, 38.0f};
		Result.m_aButtons[MENU] = {Screen.x - 62.0f, 16.0f, 44.0f, 34.0f};
		Result.m_aButtons[CHAT] = {Screen.x - 112.0f, 16.0f, 44.0f, 34.0f};
		Result.m_aButtons[PAUSE] = {Screen.x - 162.0f, 16.0f, 44.0f, 34.0f};
		return Result;
	}

	inline vec2 Direction(vec2 Offset, float Radius)
	{
		const float Length = length(Offset);
		if(!std::isfinite(Length) || Radius <= 0.0f || Length <= Radius * 0.12f)
			return vec2(0.0f, 0.0f);
		const float Amount = std::clamp((Length / Radius - 0.12f) / 0.88f, 0.0f, 1.0f);
		return Offset * (Amount / Length);
	}

	inline vec2 Movement(vec2 Direction, float Zoom, float FrameTime)
	{
		if(!std::isfinite(FrameTime) || !std::isfinite(Zoom) || Zoom <= 0.0f)
			return vec2(0.0f, 0.0f);
		return Direction * (1000.0f * Zoom * std::clamp(FrameTime, 0.0f, 0.05f));
	}

	inline void NeutralizeGameplayInput(CNetObj_PlayerInput &Input)
	{
		Input.m_Direction = 0;
		Input.m_Jump = 0;
		Input.m_Hook = 0;
		Input.m_Fire = (Input.m_Fire + (Input.m_Fire & 1)) & INPUT_STATE_MASK;
		Input.m_WantedWeapon = 0;
		Input.m_NextWeapon = 0;
		Input.m_PrevWeapon = 0;
	}

	class CTouchState
	{
		std::vector<IInput::CTouchFinger> m_vPrevious;
		std::optional<IInput::CTouchFinger> m_JoystickFinger;
		vec2 m_Direction = vec2(0.0f, 0.0f);
		bool m_WaitForRelease = false;
		bool m_OpenSelectorOnRelease = false;

	public:
		void Cancel()
		{
			m_JoystickFinger.reset();
			m_Direction = vec2(0.0f, 0.0f);
			m_WaitForRelease = true;
			m_OpenSelectorOnRelease = false;
		}

		vec2 JoystickDirection() const { return m_Direction; }

		unsigned Update(const std::vector<IInput::CTouchFingerState> &vFingers, const SLayout &Layout, vec2 Screen)
		{
			unsigned Actions = 0;
			m_Direction = vec2(0.0f, 0.0f);
			if(m_WaitForRelease)
			{
				if(!vFingers.empty())
					return 0;
				m_WaitForRelease = false;
			}
			bool JoystickPresent = false;
			for(const auto &Finger : vFingers)
			{
				const vec2 Position = Finger.m_Position * Screen;
				const bool NewPress = std::find(m_vPrevious.begin(), m_vPrevious.end(), Finger.m_Finger) == m_vPrevious.end();
				if(NewPress && !m_JoystickFinger.has_value() && distance(Position, Layout.m_Joystick.Center()) <= Layout.m_Joystick.w * 0.5f)
					m_JoystickFinger = Finger.m_Finger;
				if(m_JoystickFinger.has_value() && Finger.m_Finger == *m_JoystickFinger)
				{
					JoystickPresent = true;
					m_Direction = Direction(Position - Layout.m_Joystick.Center(), Layout.m_Joystick.w * 0.5f);
				}
				else if(NewPress)
				{
					for(int i = 0; i < NUM_ACTIONS; ++i)
						if(Layout.m_aButtons[i].Inside(Position))
						{
							if(i == SELECT)
								m_OpenSelectorOnRelease = true;
							else
								Actions |= 1U << i;
						}
				}
			}
			// 选择器会读取原始触摸；等打开按钮的手指松开，避免同次点击误选或关闭面板。
			if(m_OpenSelectorOnRelease)
			{
				m_Direction = vec2(0.0f, 0.0f);
				m_JoystickFinger.reset();
				if(vFingers.empty())
				{
					Actions |= 1U << SELECT;
					m_OpenSelectorOnRelease = false;
				}
			}
			if(!JoystickPresent)
				m_JoystickFinger.reset();
			m_vPrevious.clear();
			for(const auto &Finger : vFingers)
				m_vPrevious.push_back(Finger.m_Finger);
			return Actions;
		}
	};

	class CJoinState
	{
		double m_NextRequest = 0.0;

	public:
		void Reset() { m_NextRequest = 0.0; }
		bool Update(bool Online, bool HasLocalInfo, bool Spectating, double Now)
		{
			if(!Online || Spectating)
			{
				Reset();
				return false;
			}
			if(!HasLocalInfo || Now < m_NextRequest)
				return false;
			m_NextRequest = Now + 3.0;
			return true;
		}
	};
}

#endif
