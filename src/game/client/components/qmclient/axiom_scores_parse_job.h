#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_SCORES_PARSE_JOB_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_SCORES_PARSE_JOB_H

#include "axiom_scores.h"

#include <engine/shared/jobs.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// 持有已完成的响应，解析期间不借用组件；主线程只接收完整结果。
class CQmAxiomDdStatsParseJob : public IJob
{
	std::shared_ptr<IQmAxiomHttpRequest> m_pRequest;
	std::string m_PlayerName;
	std::atomic<bool> m_Ready{false};
	EQmAxiomParseResult m_Result = EQmAxiomParseResult::INVALID_RESPONSE;
	std::vector<SQmDdStatsGameType> m_vGameTypes;

	void Run() override { Parse(); }

public:
	CQmAxiomDdStatsParseJob(std::shared_ptr<IQmAxiomHttpRequest> pRequest, std::string PlayerName) :
		m_pRequest(std::move(pRequest)),
		m_PlayerName(std::move(PlayerName)) {}

	void Parse()
	{
		const unsigned char *pData = nullptr;
		size_t DataSize = 0;
		m_pRequest->Result(&pData, &DataSize);
		m_Result = QmParseDdStatsPlayerResponse(reinterpret_cast<const char *>(pData), DataSize, m_PlayerName.c_str(), m_vGameTypes);
		m_Ready.store(true, std::memory_order_release);
	}

	bool Ready() const { return m_Ready.load(std::memory_order_acquire); }
	EQmAxiomParseResult Result() const { return m_Result; }
	std::vector<SQmDdStatsGameType> TakeGameTypes() { return std::move(m_vGameTypes); }
};

#endif
