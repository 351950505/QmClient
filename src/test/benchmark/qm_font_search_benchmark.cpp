// 固定字体列表，分别计量输入变化时过滤和查询未变时的缓存路径；不读系统目录。
#include <game/client/QmUi/QmDropdown.h>

#include <benchmark/benchmark.h>

static void FontSearchFilter(benchmark::State &State)
{
	std::vector<std::string> vNames;
	for(int Index = 0; Index < State.range(0); ++Index)
		vNames.push_back("Font " + std::to_string(Index));
	CQmDropdownSearch Search;
	if(!Search.Update(vNames, "Font") || Search.Indices().size() != vNames.size())
	{
		State.SkipWithError("Font filter precheck failed");
		return;
	}
	bool Toggle = false;
	for(auto _ : State)
	{
		Toggle = !Toggle;
		benchmark::DoNotOptimize(Search.Update(vNames, Toggle ? "Font 1" : "Font 2"));
		benchmark::DoNotOptimize(Search.Indices().size());
	}
	State.SetItemsProcessed(State.iterations() * vNames.size());
}

static void FontSearchCached(benchmark::State &State)
{
	std::vector<std::string> vNames;
	for(int Index = 0; Index < State.range(0); ++Index)
		vNames.push_back("Font " + std::to_string(Index));
	CQmDropdownSearch Search;
	Search.Update(vNames, "Font 1");
	if(Search.Update(vNames, "Font 1"))
	{
		State.SkipWithError("Font search cache precheck failed");
		return;
	}
	for(auto _ : State)
		benchmark::DoNotOptimize(Search.Update(vNames, "Font 1"));
	State.SetItemsProcessed(State.iterations());
}

BENCHMARK(FontSearchFilter)->Arg(32)->Arg(512)->Arg(2048);
BENCHMARK(FontSearchCached)->Arg(32)->Arg(512)->Arg(2048);
