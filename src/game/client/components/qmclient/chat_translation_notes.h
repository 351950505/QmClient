#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_CHAT_TRANSLATION_NOTES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_CHAT_TRANSLATION_NOTES_H

#include <base/str.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

// 服务器回显只携带译文；原文只在本客户端以有界、短期附注等待匹配。
class COutgoingChatAnnotations
{
	struct SEntry
	{
		int m_Connection;
		int m_ClientId;
		int m_Team;
		std::string m_Text;
		std::string m_Original;
		int64_t m_Time;
		int64_t m_Lifetime;
		bool m_Ambiguous = false;
	};
	std::deque<SEntry> m_Entries;
	int64_t m_LastTime = -1;

	static std::string Normalize(const char *pText)
	{
		std::string Text = pText != nullptr ? pText : "";
		if(!Text.empty())
		{
			str_utf8_trim_right(Text.data());
			Text.resize(str_length(Text.c_str()));
		}
		return Text;
	}
	void Prune(int64_t Now)
	{
		if(Now < m_LastTime)
			Clear();
		m_LastTime = Now;
		m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(), [Now](const SEntry &Entry) {
			return Now >= Entry.m_Time && static_cast<uint64_t>(Now) - static_cast<uint64_t>(Entry.m_Time) >= static_cast<uint64_t>(Entry.m_Lifetime);
		}),
			m_Entries.end());
	}

public:
	static constexpr size_t CAPACITY = 32;
	void Clear()
	{
		m_Entries.clear();
		m_LastTime = -1;
	}
	size_t Size() const { return m_Entries.size(); }
	void InvalidateConnection(int Connection)
	{
		m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(), [Connection](const SEntry &Entry) { return Entry.m_Connection == Connection; }), m_Entries.end());
	}
	void Record(int Connection, int ClientId, int Team, const char *pText, const char *pOriginal, int64_t Now, int64_t Lifetime)
	{
		Prune(Now);
		if(Connection < 0 || Connection > 1 || ClientId < 0 || Now < 0 || Lifetime <= 0 || pText == nullptr || str_length(pText) > 4096 || (pOriginal != nullptr && str_length(pOriginal) > 4096))
			return;
		std::string Text = Normalize(pText);
		if(Text.empty())
			return;
		const auto Existing = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const SEntry &Entry) {
			return Entry.m_Connection == Connection && Entry.m_ClientId == ClientId && Entry.m_Team == Team && Entry.m_Text == Text;
		});
		if(Existing != m_Entries.end())
		{
			// 网络发送成功不等于服务器接纳。同文未回显时无法判定归属，保留歧义墓碑直到过期。
			Existing->m_Ambiguous = true;
			Existing->m_Original.clear();
			Existing->m_Time = Now;
			Existing->m_Lifetime = Lifetime;
			return;
		}
		if(m_Entries.size() == CAPACITY)
			m_Entries.pop_front();
		// 普通消息也登记空附注，避免相同译文的较早普通回显误领取后来的原文。
		m_Entries.push_back({Connection, ClientId, Team, std::move(Text), pOriginal != nullptr ? pOriginal : "", Now, Lifetime});
	}
	std::string Consume(int Connection, int ClientId, int Team, const char *pText, int64_t Now)
	{
		Prune(Now);
		const std::string Text = Normalize(pText);
		const auto It = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const SEntry &Entry) {
			return Entry.m_Connection == Connection && Entry.m_ClientId == ClientId && Entry.m_Team == Team && Entry.m_Text == Text;
		});
		if(It == m_Entries.end() || It->m_Ambiguous)
			return {};
		std::string Original = std::move(It->m_Original);
		m_Entries.erase(It);
		return Original;
	}
};

#endif
