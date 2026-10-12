#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_PENDING_CHAT_MESSAGES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_PENDING_CHAT_MESSAGES_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>

// 待发消息独立持有正文与本地原文，不能从输入历史倒推发送队列。
class CPendingChatMessages
{
public:
	struct SMessage
	{
		int m_Team;
		int m_Connection = -1;
		int m_ClientId = -1;
		std::string m_Text;
		std::string m_Original;
	};

	// 前三项顺序与 CChat::EChatSendResult 一致。
	enum class EAdmission
	{
		REJECTED,
		SENT,
		QUEUED,
	};

	static constexpr size_t CAPACITY = 3;

private:
	std::deque<SMessage> m_Messages;
	int64_t m_LastObservedTime = -1;

	static bool ValidTime(int64_t LastSend, int64_t Now, int64_t Frequency)
	{
		return LastSend >= 0 && Now >= LastSend && Frequency > 0;
	}

	static bool Ready(int64_t LastSend, int64_t Now, int64_t Frequency)
	{
		// 非负且有序的时间先作差，避免 LastSend + Frequency 溢出。
		return Now - LastSend > Frequency;
	}

	bool ObserveTime(int64_t LastSend, int64_t Now, int64_t Frequency)
	{
		if(!ValidTime(LastSend, Now, Frequency) || Now < m_LastObservedTime)
		{
			// 时钟异常不能补发旧消息；清空后允许后续正常调用重新接纳。
			Clear();
			return false;
		}
		m_LastObservedTime = Now;
		return true;
	}

public:
	// 无实例的额度入口共用接纳策略；跨调用时钟回退由实例入口检测。
	static EAdmission Reserve(int64_t LastSend, int64_t Now, int64_t Frequency, int &PendingCount)
	{
		if(!ValidTime(LastSend, Now, Frequency) || PendingCount < 0 || PendingCount > static_cast<int>(CAPACITY))
			return EAdmission::REJECTED;
		if(PendingCount == 0 && Ready(LastSend, Now, Frequency))
			return EAdmission::SENT;
		if(PendingCount == static_cast<int>(CAPACITY))
			return EAdmission::REJECTED;
		++PendingCount;
		return EAdmission::QUEUED;
	}

	// SENT 不保存 Message，调用者负责发送仍由自己持有的消息。
	EAdmission Admit(int64_t LastSend, int64_t Now, int64_t Frequency, SMessage Message)
	{
		if(!ObserveTime(LastSend, Now, Frequency))
			return EAdmission::REJECTED;
		int PendingCount = static_cast<int>(m_Messages.size());
		const EAdmission Admission = Reserve(LastSend, Now, Frequency, PendingCount);
		if(Admission == EAdmission::QUEUED)
			m_Messages.push_back(std::move(Message));
		return Admission;
	}

	// 弹出不更新发送时间，调用者在实际发送时维护 LastSend。
	std::optional<SMessage> PopReady(int64_t LastSend, int64_t Now, int64_t Frequency)
	{
		if(!ObserveTime(LastSend, Now, Frequency) || m_Messages.empty() || !Ready(LastSend, Now, Frequency))
			return std::nullopt;
		SMessage Message = std::move(m_Messages.front());
		m_Messages.pop_front();
		return Message;
	}

	void Clear()
	{
		m_Messages.clear();
		m_LastObservedTime = -1;
	}

	size_t Size() const { return m_Messages.size(); }

	void InvalidateConnection(int Connection)
	{
		m_Messages.erase(std::remove_if(m_Messages.begin(), m_Messages.end(), [Connection](const SMessage &Message) {
			return Message.m_Connection == Connection;
		}),
			m_Messages.end());
	}
};

#endif
