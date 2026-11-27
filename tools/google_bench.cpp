#include <benchmark/benchmark.h>
#include "systems/SpatialIndex.h"
#include "systems/Navigation.h"
#include <random>
using namespace eng::systems;
static std::vector<SpatialItem> Items(int count) {
    std::mt19937 random(42); std::uniform_real_distribution<float> dist(-100,100);
    std::vector<SpatialItem> items;
    for(int i=0;i<count;++i) { glm::vec3 c(dist(random),dist(random),dist(random)); items.push_back({static_cast<std::uint64_t>(i+1),{c-glm::vec3(1),c+glm::vec3(1)}}); }
    return items;
}
static void BM_Brute(benchmark::State& state) { auto items=Items(static_cast<int>(state.range(0))); Ray ray{{0,0,150},{0,0,-1},300}; for(auto _:state) benchmark::DoNotOptimize(BruteRaycast(items,ray)); }
static void BM_Bvh(benchmark::State& state) { SpatialIndex bvh; bvh.Build(Items(static_cast<int>(state.range(0)))); Ray ray{{0,0,150},{0,0,-1},300}; for(auto _:state) benchmark::DoNotOptimize(bvh.Raycast(ray)); }
static void BM_Build(benchmark::State& state) { auto items=Items(static_cast<int>(state.range(0))); for(auto _:state) { SpatialIndex bvh; bvh.Build(items); benchmark::DoNotOptimize(bvh.Size()); } }
static void BM_Path(benchmark::State& state) { Grid grid{128,128,std::vector<std::uint8_t>(128*128)}; for(auto _:state) benchmark::DoNotOptimize(FindPath(grid,{0,0},{127,127},state.range(0)!=0).expanded); }
BENCHMARK(BM_Brute)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Bvh)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Build)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Path)->Arg(0)->Arg(1);
