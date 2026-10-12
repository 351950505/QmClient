#ifndef GAME_CLIENT_QMUI_QMLISTENTRYANIMATION_H
#define GAME_CLIENT_QMUI_QMLISTENTRYANIMATION_H

#include <algorithm>
#include <cstdint>

enum class EQmListEntryRenderPass
{
	INTERACTIVE,
	BACKGROUND,
	MEASUREMENT,
};

constexpr bool QmListBoxShouldStartEntryAnimation(bool Enabled, bool MeasurementOnly, int64_t LastRenderTime, int64_t Now, int64_t InactiveGap)
{
	return Enabled && !MeasurementOnly && (LastRenderTime == 0 || Now - LastRenderTime > InactiveGap);
}

inline float QmListBoxEntryOffset(float ElapsedSeconds, float DurationSeconds, float Distance)
{
	if(DurationSeconds <= 0.0f || ElapsedSeconds >= DurationSeconds)
		return 0.0f;
	const float Progress = std::clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
	const float EasedProgress = 1.0f - (1.0f - Progress) * (1.0f - Progress);
	return -Distance * (1.0f - EasedProgress);
}

constexpr bool QmListBoxEntryAnimationFinished(bool Enabled, bool MeasurementOnly, float ElapsedSeconds, float DurationSeconds)
{
	return !Enabled || (!MeasurementOnly && (DurationSeconds <= 0.0f || ElapsedSeconds >= DurationSeconds));
}

// 可见列表共用同一入场生命周期；测量只读取布局，不延长可见时间。
class CQmListEntryAnimation
{
	int64_t m_LastRenderTime = 0;
	int64_t m_StartTime = 0;

public:
	void Reset()
	{
		m_LastRenderTime = 0;
		m_StartTime = 0;
	}

	float Resolve(int64_t Now, int64_t Frequency, bool Enabled, int MotionLevel, EQmListEntryRenderPass Pass)
	{
		Frequency = std::max<int64_t>(1, Frequency);
		Enabled = Enabled && MotionLevel > 0;
		const bool MeasurementOnly = Pass == EQmListEntryRenderPass::MEASUREMENT;
		if(!Enabled && !MeasurementOnly)
			m_StartTime = 0;
		else if(QmListBoxShouldStartEntryAnimation(Enabled, MeasurementOnly, m_LastRenderTime, Now, Frequency * 2 / 5))
			m_StartTime = Now;
		float Offset = 0.0f;
		if(!MeasurementOnly && Enabled && m_StartTime > 0)
		{
			const float Duration = MotionLevel == 1 ? 0.10f : 0.16f;
			const float Distance = MotionLevel == 1 ? 6.0f : 12.0f;
			const float Elapsed = static_cast<float>(Now - m_StartTime) / static_cast<float>(Frequency);
			Offset = QmListBoxEntryOffset(Elapsed, Duration, Distance);
			if(QmListBoxEntryAnimationFinished(Enabled, MeasurementOnly, Elapsed, Duration))
				m_StartTime = 0;
		}
		if(!MeasurementOnly)
			m_LastRenderTime = Now;
		return Offset;
	}
};

#endif
