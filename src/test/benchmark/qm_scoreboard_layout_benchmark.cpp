#include <game/client/components/qmclient/scoreboard_layout.h>

#include <benchmark/benchmark.h>

// 玩家行准备在计时外，每次冷算包含队伍边界检查及整份分列计划。
static void BM_QmScoreboardColumns(benchmark::State &State)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	const int Count = static_cast<int>(State.range(0));
	for(int i = 0; i < Count; ++i)
		aRows[i] = {i < 16 ? TEAM_FLOCK : 1 + (i - 16) / 4, i >= 16 && (i - 16) % 4 == 0, false};
	for(auto _ : State)
	{
		benchmark::DoNotOptimize(aRows);
		const auto Columns = QmScoreboardColumns(aRows, Count, 1050.0f, 380.0f, 310.0f);
		benchmark::DoNotOptimize(Columns);
	}
	State.SetItemsProcessed(State.iterations() * Count);
}
BENCHMARK(BM_QmScoreboardColumns)->Arg(16)->Arg(81)->Arg(127);

// 预热不计时，稳态每帧仍检查行计划和视口；测量真实生产缓存入口。
static void BM_QmScoreboardLayoutCacheHit(benchmark::State &State)
{
	std::array<SQmScoreboardLayoutRow, MAX_CLIENTS> aRows{};
	const int Count = static_cast<int>(State.range(0));
	CQmScoreboardLayoutCache Cache;
	Cache.Resolve(aRows, Count, 1050.0f, 380.0f, 310.0f);
	for(auto _ : State)
	{
		benchmark::DoNotOptimize(aRows);
		benchmark::DoNotOptimize(Cache.Resolve(aRows, Count, 1050.0f, 380.0f, 310.0f));
	}
	State.SetItemsProcessed(State.iterations() * Count);
}
BENCHMARK(BM_QmScoreboardLayoutCacheHit)->Arg(16)->Arg(81)->Arg(127);
