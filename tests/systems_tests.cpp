#include "systems/AssetStream.h"
#include "systems/Navigation.h"
#include "systems/Netcode.h"
#include "systems/SpatialIndex.h"
#include <glm/geometric.hpp>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace eng::systems;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #x); } while (false)
template<class F> void Throws(F fn) { bool caught = false; try { fn(); } catch (const std::exception&) { caught = true; } CHECK(caught); }

static void Jobs() {
    for (std::size_t workers : {1u, 4u}) {
        JobSystem jobs(workers);
        std::atomic<int> total{0}; std::vector<std::future<int>> results;
        for (int i = 0; i < 2000; ++i) results.push_back(jobs.Submit([&, i] { ++total; return i * i; }));
        for (int i = 0; i < 2000; ++i) CHECK(jobs.Wait(results[i]) == i*i);
        CHECK(total == 2000);
        auto outer = jobs.Submit([&] { auto inner = jobs.Submit([] { return 42; }); return jobs.Wait(inner); });
        CHECK(jobs.Wait(outer) == 42);
        auto fail = jobs.Submit([]() -> int { throw std::runtime_error("expected"); });
        Throws([&] { jobs.Wait(fail); });
        std::vector<std::thread> producers;
        for (int p = 0; p < 4; ++p) producers.emplace_back([&] { for (int n = 0; n < 500; ++n) jobs.Submit([&] { ++total; }); });
        for (auto& t : producers) t.join();
        jobs.WaitIdle(); CHECK(total == 4000);
        auto missing = StreamAsset(jobs, "definitely-missing-systems-test.asset");
        Throws([&] { jobs.Wait(missing); });
        const auto file = std::filesystem::temp_directory_path() / ("gd-stream-" + std::to_string(std::random_device{}()) + ".bin");
        { std::ofstream out(file, std::ios::binary); out << "asset bytes"; }
        auto asset = StreamAsset(jobs, file);
        CHECK(jobs.Wait(asset).size() == 11);
        auto limited = StreamAsset(jobs, file, 2); Throws([&] { jobs.Wait(limited); });
        std::filesystem::remove(file);
    }
    std::atomic<int> drained{0};
    { JobSystem jobs(4); for (int i = 0; i < 1000; ++i) jobs.Submit([&] { ++drained; }); }
    CHECK(drained == 1000);
}
static void Spatial() {
    std::mt19937 rng(42); std::uniform_real_distribution<float> pos(-100,100);
    std::vector<SpatialItem> items;
    for (std::uint64_t i = 1; i <= 1000; ++i) { glm::vec3 p(pos(rng),pos(rng),pos(rng)); items.push_back({i,{p-glm::vec3(2),p+glm::vec3(2)}}); }
    SpatialIndex bvh; bvh.Build(items);
    for (int i = 0; i < 2000; ++i) {
        Ray ray{{pos(rng),pos(rng),pos(rng)}, glm::normalize(glm::vec3(pos(rng),pos(rng),pos(rng))), 300};
        auto a = BruteRaycast(items,ray), b = bvh.Raycast(ray);
        CHECK(a.hit == b.hit); if (a.hit) { CHECK(a.id == b.id); CHECK(std::abs(a.distance-b.distance) < 1e-5); }
    }
    bvh.Build({{2,{{-1,-1,-1},{1,1,1}}},{1,{{-1,-1,-1},{1,1,1}}}});
    CHECK(bvh.Raycast({{0,0,0},{0,0,1},10}).id == 1);
    CHECK(bvh.Raycast({{0,0,0},{0,0,1},10}).distance == 0);
    CHECK(!bvh.Raycast({{2,0,0},{0,0,1},10}).hit);
    CHECK(!bvh.Raycast({{0,0,0},{0,0,0},10}).hit);
    CHECK(!bvh.Raycast({{0,0,4},{0,0,-1},1}).hit);
    CHECK(bvh.Candidates({{0,0,4},{0,0,-1},10}).size() == 2);
    CHECK(!bvh.Raycast({{0,0,0},{NAN,0,1},10}).hit);
    Throws([&] { bvh.Build({{1,{{2,0,0},{1,1,1}}}}); });
    bvh.Build({}); CHECK(!bvh.Raycast({}).hit);
}
static void Navigation() {
    std::mt19937 rng(17);
    for (int run = 0; run < 100; ++run) {
        Grid grid{24,24,std::vector<std::uint8_t>(24*24)};
        for (auto& cell : grid.blocked) cell = rng()%5 == 0;
        grid.blocked.front() = grid.blocked.back() = 0;
        auto a = FindPath(grid,{0,0},{23,23}), b = FindPath(grid,{0,0},{23,23},false);
        CHECK(a.cells.size() == b.cells.size());
        for (std::size_t i = 1; i < a.cells.size(); ++i) {
            CHECK(grid.Walkable(a.cells[i]));
            CHECK(std::abs(a.cells[i].x-a.cells[i-1].x)+std::abs(a.cells[i].y-a.cells[i-1].y) == 1);
        }
    }
    CHECK(FindPath({}, {}, {}).cells.empty());
    Grid one{1,1,{0}}; CHECK(FindPath(one,{0,0},{0,0}).cells.size() == 1);
    one.blocked[0] = 1; CHECK(FindPath(one,{0,0},{0,0}).cells.empty());
    CHECK(FindPath({2,2,{0}}, {}, {}).cells.empty());
}
static void Network() {
    using namespace net;
    auto bytes = Encode(Input{1, .25f, -1}); auto decoded = DecodeInput(bytes);
    CHECK(decoded && decoded->sequence == 1 && decoded->x == .25f && decoded->z == -1);
    for (std::size_t i = 0; i < bytes.size(); ++i) CHECK(!DecodeInput({bytes.begin(),bytes.begin()+i}));
    bytes[2] = 7; CHECK(!DecodeInput(bytes));
    bytes = Encode(Input{1,0,0}); bytes[8] = 0x7f; bytes[9] = 0xc0; CHECK(!DecodeInput(bytes));
    Throws([] { Encode(Input{0,0,0}); });
    Prediction client; Authority server; std::vector<Input> commands;
    for (int i = 0; i < 120; ++i) commands.push_back(client.Push(i < 60 ? 1.0f : 0.0f, i >= 60 ? 1.0f : 0.0f));
    // Deliberately reorder and duplicate; authority must apply each input once.
    for (auto i = commands.rbegin(); i != commands.rend(); ++i) server.Receive(*i);
    CHECK(server.State().acknowledged == 120);
    CHECK(!server.Receive(commands.front()));
    CHECK(client.Reconcile(server.State())); CHECK(client.Pending().empty());
    CHECK(std::abs(client.GetPosition().x - 6) < 1e-4);
    CHECK(std::abs(client.GetPosition().z - 6) < 1e-4);
    CHECK(!client.Reconcile({121,121,{0,0}}));
    auto next = client.Push(1,0); CHECK(next.sequence == 121);
    CHECK(client.Reconcile({120,120,{0,0}}) == false); // stale correction
    CHECK(!server.Receive({5000,1,0}));
    Prediction replay; replay.Push(1,0); replay.Push(1,0);
    CHECK(replay.Reconcile({1,1,{5,0}})); CHECK(std::abs(replay.GetPosition().x - 5.1f) < 1e-5);
    Interpolation interpolation; interpolation.Push({20,20,{20,0}}); interpolation.Push({10,10,{10,0}});
    CHECK(interpolation.Sample(15).x == 15); CHECK(interpolation.Sample(-1).x == 10); CHECK(interpolation.Sample(100).x == 20);
    Prediction bounded; for (std::size_t i=0;i<MaxPending;++i) bounded.Push(0,0);
    Throws([&] { bounded.Push(0,0); });
    auto snapshot = DecodeSnapshot(Encode(server.State())); CHECK(snapshot && snapshot->acknowledged == 120);
    UdpSocket sender, receiver;
    CHECK(sender.Send({"127.0.0.1",receiver.Port()},Encode(Input{1,1,0})));
    std::optional<Datagram> packet;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!packet && std::chrono::steady_clock::now() < deadline) { packet=receiver.Receive(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    CHECK(packet && packet->source.port == sender.Port() && DecodeInput(packet->bytes));
    Throws([] { Impairment(-1,0,0,1); });
}
int main() {
    try { Jobs(); Spatial(); Navigation(); Network(); std::cout << checks << " systems checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
