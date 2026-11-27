#include "systems/Navigation.h"
#include "systems/Profiler.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>

namespace eng::systems {
bool Grid::Valid() const {
    return width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
        blocked.size() == static_cast<std::size_t>(width) * height;
}
bool Grid::Walkable(Cell c) const {
    return Valid() && c.x >= 0 && c.y >= 0 && c.x < width && c.y < height && !blocked[c.y * width + c.x];
}
PathResult FindPath(const Grid& grid, Cell start, Cell goal, bool heuristic) {
    GD_ZONE("Grid pathfinding");
    PathResult result;
    if (!grid.Walkable(start) || !grid.Walkable(goal)) return result;
    auto index = [&](Cell c) { return c.y * grid.width + c.x; };
    auto estimate = [&](Cell c) { return heuristic ? std::abs(c.x - goal.x) + std::abs(c.y - goal.y) : 0; };
    struct Entry { int f, g, id; bool operator<(const Entry& b) const {
        if (f != b.f) return f > b.f;
        if (g != b.g) return g < b.g; // prefer progress on equal f
        return id > b.id;
    }};
    std::priority_queue<Entry> open;
    std::vector<int> cost(grid.blocked.size(), std::numeric_limits<int>::max()), parent(grid.blocked.size(), -1);
    const int first = index(start), last = index(goal);
    cost[first] = 0; open.push({estimate(start), 0, first});
    constexpr Cell directions[] = {{1,0}, {0,1}, {-1,0}, {0,-1}};
    while (!open.empty()) {
        const auto top = open.top(); open.pop();
        if (cost[top.id] != top.g) continue;
        ++result.expanded;
        if (top.id == last) {
            for (int id = last; id != -1; id = parent[id]) result.cells.push_back({id % grid.width, id / grid.width});
            std::reverse(result.cells.begin(), result.cells.end()); return result;
        }
        const Cell here{top.id % grid.width, top.id / grid.width};
        for (auto d : directions) {
            const Cell next{here.x + d.x, here.y + d.y};
            if (!grid.Walkable(next)) continue;
            const int id = index(next), g = top.g + 1;
            if (g >= cost[id]) continue;
            cost[id] = g; parent[id] = top.id; open.push({g + estimate(next), g, id});
        }
    }
    return result;
}
}
