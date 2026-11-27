#pragma once
#include <cstdint>
#include <vector>

namespace eng::systems {
struct Cell { int x = 0, y = 0; bool operator==(Cell b) const { return x == b.x && y == b.y; } };
struct Grid {
    int width = 0, height = 0;
    std::vector<std::uint8_t> blocked;
    bool Valid() const;
    bool Walkable(Cell c) const;
};
struct PathResult { std::vector<Cell> cells; std::size_t expanded = 0; };
// Four-neighbour unit-cost grid; Manhattan heuristic is admissible.
// Set heuristic=false for Dijkstra comparison on identical input.
PathResult FindPath(const Grid& grid, Cell start, Cell goal, bool heuristic = true);
}
