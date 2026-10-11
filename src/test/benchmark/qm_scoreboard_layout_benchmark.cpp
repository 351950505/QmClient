#include <game/client/components/qmclient/scoreboard_layout.h>

#include <benchmark/benchmark.h>

// 测量生产行指标计算，容量沿原多人分列策略；标签与模式图标占位包含在计时内。
static void BM_QmScoreboardRowMetrics(benchmark::State &State)
{
	int Capacity = static_cast<int>(State.range(0));
	float Height = 388.0f;
	for(auto _ : State)
	{
		benchmark::DoNotOptimize(Capacity);
		benchmark::DoNotOptimize(Height);
		const auto Metrics = QmScoreboardRowMetrics(Height, Capacity, Capacity / 4, Capacity / 8, false);
		benchmark::DoNotOptimize(Metrics);
	}
	State.SetItemsProcessed(State.iterations());
}
BENCHMARK(BM_QmScoreboardRowMetrics)->Arg(16)->Arg(24)->Arg(32)->Arg(43);
