#include "systems/Navigation.h"
#include "systems/SpatialIndex.h"
#include "systems/JobSystem.h"
#include <glm/geometric.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>

using namespace eng::systems;
using Clock=std::chrono::steady_clock;
static volatile std::uint64_t sink=0;
template<class F> void Measure(const char* name, int size, F fn) {
    for(int i=0;i<5;++i) sink=fn();
    std::vector<double> times;
    for(int i=0;i<100;++i) { auto start=Clock::now(); sink=fn(); times.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count()); }
    const auto mean=std::accumulate(times.begin(),times.end(),0.0)/times.size(); std::sort(times.begin(),times.end());
    std::cout<<name<<','<<size<<','<<mean<<','<<times[98]<<','<<times.size()<<'\n';
}
int main() {
    std::cout<<"algorithm,size,mean_us,p99_us,samples\n"<<std::setprecision(8);
    for(int count : {1000,10000}) {
        std::mt19937 rng(42); std::uniform_real_distribution<float> p(-100,100);
        std::vector<SpatialItem> items; std::vector<Ray> rays;
        for(int i=0;i<count;++i) { glm::vec3 c(p(rng),p(rng),p(rng)); items.push_back({static_cast<std::uint64_t>(i+1),{c-glm::vec3(1),c+glm::vec3(1)}}); }
        for(int i=0;i<128;++i) rays.push_back({{p(rng),p(rng),p(rng)},glm::normalize(glm::vec3(p(rng),p(rng),p(rng))),300});
        SpatialIndex bvh; bvh.Build(items);
        Measure("brute_128_rays",count,[&] { std::uint64_t sum=0; for(auto ray:rays) sum+=BruteRaycast(items,ray).id; return sum; });
        Measure("bvh_128_rays",count,[&] { std::uint64_t sum=0; for(auto ray:rays) sum+=bvh.Raycast(ray).id; return sum; });
        Measure("bvh_build",count,[&] { bvh.Build(items); return bvh.Size(); });
    }
    Grid grid{128,128,std::vector<std::uint8_t>(128*128)};
    for(int y=0;y<127;++y) grid.blocked[y*128+64]=1;
    Measure("astar",128,[&] { return FindPath(grid,{0,0},{127,127}).cells.size(); });
    Measure("dijkstra",128,[&] { return FindPath(grid,{0,0},{127,127},false).cells.size(); });
    JobSystem jobs(4);
    Measure("jobs_256_dispatch_wait",256,[&] { std::vector<std::future<int>> tasks; for(int i=0;i<256;++i) tasks.push_back(jobs.Submit([i]{return i;})); std::uint64_t sum=0; for(auto& task:tasks) sum+=jobs.Wait(task); return sum; });
}
