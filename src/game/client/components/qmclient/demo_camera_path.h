#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_PATH_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_PATH_H

#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace qm_demo_camera
{

	enum class ETransition
	{
		SMOOTH,
		LINEAR,
		HOLD,
	};

	struct SView
	{
		vec2 m_Center{};
		float m_Zoom = 1.0f;
	};

	struct SKeyframe
	{
		double m_Tick = 0.0;
		SView m_View;
		ETransition m_Transition = ETransition::SMOOTH;
		// 裁剪只截取原曲线的参数区间，保留片段内部原有的运镜速度。
		double m_EaseFrom = 0.0;
		double m_EaseTo = 1.0;
	};

	struct SRange
	{
		int m_StartTick;
		int m_EndTick;
	};

	class CTrack
	{
		std::vector<SKeyframe> m_vFrames;
		float m_Aspect = 16.0f / 9.0f;

		static double Ease(double Value) { return Value * Value * (3.0 - 2.0 * Value); }

	public:
		static constexpr size_t MAX_KEYFRAMES = 512;
		const std::vector<SKeyframe> &Frames() const { return m_vFrames; }
		float Aspect() const { return m_Aspect; }
		bool Empty() const { return m_vFrames.empty(); }
		bool SetAspect(float Aspect)
		{
			if(!std::isfinite(Aspect) || Aspect < 0.1f || Aspect > 10.0f)
				return false;
			m_Aspect = Aspect;
			return true;
		}

		static bool Valid(const SKeyframe &Frame)
		{
			return std::isfinite(Frame.m_Tick) && Frame.m_Tick >= 0.0 && Frame.m_Tick <= std::numeric_limits<int>::max() &&
			       std::isfinite(Frame.m_View.m_Center.x) && std::abs(Frame.m_View.m_Center.x) <= 10000000.0f &&
			       std::isfinite(Frame.m_View.m_Center.y) && std::abs(Frame.m_View.m_Center.y) <= 10000000.0f &&
			       std::isfinite(Frame.m_View.m_Zoom) && Frame.m_View.m_Zoom >= 0.01f && Frame.m_View.m_Zoom <= 240.0f &&
			       Frame.m_Transition >= ETransition::SMOOTH && Frame.m_Transition <= ETransition::HOLD &&
			       std::isfinite(Frame.m_EaseFrom) && std::isfinite(Frame.m_EaseTo) &&
			       Frame.m_EaseFrom >= 0.0 && Frame.m_EaseFrom < Frame.m_EaseTo && Frame.m_EaseTo <= 1.0;
		}

		int Insert(SKeyframe Frame)
		{
			if(!Valid(Frame))
				return -1;
			auto It = std::lower_bound(m_vFrames.begin(), m_vFrames.end(), Frame.m_Tick,
				[](const SKeyframe &Existing, double Tick) { return Existing.m_Tick < Tick; });
			const int Index = static_cast<int>(It - m_vFrames.begin());
			if(It != m_vFrames.end() && It->m_Tick == Frame.m_Tick)
				*It = Frame;
			else
			{
				if(m_vFrames.size() >= MAX_KEYFRAMES)
					return -1;
				m_vFrames.insert(It, Frame);
			}
			return Index;
		}

		bool Erase(int Index)
		{
			if(Index < 0 || Index >= static_cast<int>(m_vFrames.size()))
				return false;
			m_vFrames.erase(m_vFrames.begin() + Index);
			return true;
		}

		std::optional<SView> Sample(double Tick) const
		{
			if(Empty() || !std::isfinite(Tick))
				return std::nullopt;
			const auto Next = std::upper_bound(m_vFrames.begin(), m_vFrames.end(), Tick,
				[](double Time, const SKeyframe &Frame) { return Time < Frame.m_Tick; });
			if(Next == m_vFrames.begin())
				return Next->m_View;
			const auto &From = *(Next - 1);
			if(Next == m_vFrames.end() || From.m_Transition == ETransition::HOLD || Tick == From.m_Tick)
				return From.m_View;
			double Progress = (Tick - From.m_Tick) / (Next->m_Tick - From.m_Tick);
			if(From.m_Transition == ETransition::SMOOTH)
			{
				const double Start = Ease(From.m_EaseFrom);
				const double End = Ease(From.m_EaseTo);
				if(End > Start)
					Progress = (Ease(From.m_EaseFrom + Progress * (From.m_EaseTo - From.m_EaseFrom)) - Start) / (End - Start);
			}
			const float Amount = static_cast<float>(std::clamp(Progress, 0.0, 1.0));
			return SView{mix(From.m_View.m_Center, Next->m_View.m_Center, Amount), mix(From.m_View.m_Zoom, Next->m_View.m_Zoom, Amount)};
		}

		// 与 demo 编辑器一致：首段保留原 tick，后续片段依次拼接，边界相隔一个 tick。
		std::optional<CTrack> Cut(std::vector<SRange> vRanges) const
		{
			if(Empty() || vRanges.empty())
				return std::nullopt;
			std::sort(vRanges.begin(), vRanges.end(), [](const SRange &Left, const SRange &Right) {
				return Left.m_StartTick < Right.m_StartTick || (Left.m_StartTick == Right.m_StartTick && Left.m_EndTick < Right.m_EndTick);
			});
			CTrack Result;
			Result.m_Aspect = m_Aspect;
			int64_t OutputTick = vRanges.front().m_StartTick;
			for(const auto &Range : vRanges)
			{
				if(Range.m_StartTick < 0 || Range.m_EndTick < Range.m_StartTick || OutputTick + (int64_t)Range.m_EndTick - Range.m_StartTick > std::numeric_limits<int>::max())
					return std::nullopt;
				std::vector<double> vTicks{static_cast<double>(Range.m_StartTick)};
				for(const auto &Frame : m_vFrames)
					if(Frame.m_Tick > Range.m_StartTick && Frame.m_Tick < Range.m_EndTick)
						vTicks.push_back(Frame.m_Tick);
				if(Range.m_EndTick > Range.m_StartTick)
					vTicks.push_back(Range.m_EndTick);
				for(size_t i = 0; i < vTicks.size(); ++i)
				{
					const auto View = Sample(vTicks[i]);
					if(!View.has_value())
						return std::nullopt;
					SKeyframe Frame{OutputTick + vTicks[i] - Range.m_StartTick, *View, ETransition::HOLD};
					if(i + 1 < vTicks.size())
					{
						const auto Next = std::upper_bound(m_vFrames.begin(), m_vFrames.end(), vTicks[i],
							[](double Tick, const SKeyframe &Key) { return Tick < Key.m_Tick; });
						if(Next != m_vFrames.begin() && Next != m_vFrames.end())
						{
							const auto &From = *(Next - 1);
							Frame.m_Transition = From.m_Transition;
							const double Span = Next->m_Tick - From.m_Tick;
							Frame.m_EaseFrom = From.m_EaseFrom + (vTicks[i] - From.m_Tick) / Span * (From.m_EaseTo - From.m_EaseFrom);
							Frame.m_EaseTo = From.m_EaseFrom + (vTicks[i + 1] - From.m_Tick) / Span * (From.m_EaseTo - From.m_EaseFrom);
						}
					}
					if(Result.Insert(Frame) < 0)
						return std::nullopt;
				}
				OutputTick += (int64_t)Range.m_EndTick - Range.m_StartTick + 1;
			}
			return Result;
		}
	};

	// 使用 demo 的绝对时间取样，暂停、变速、跳转均不依赖上一帧或墙上时钟。
	inline double PlaybackTick(int PreviousTick, int CurrentTick, float IntraTick)
	{
		return PreviousTick + (static_cast<double>(CurrentTick) - PreviousTick) * std::clamp(IntraTick, 0.0f, 1.0f);
	}

} // namespace qm_demo_camera

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_CAMERA_PATH_H
