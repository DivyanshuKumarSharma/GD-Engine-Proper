#pragma once
#include <glm/vec3.hpp>
#include <cstdint>
#include <vector>

namespace eng::systems {
struct Bounds { glm::vec3 min{0}, max{0}; };
struct SpatialItem { std::uint64_t id = 0; Bounds bounds; };
struct Ray { glm::vec3 origin{0}, direction{0, 0, -1}; float limit = 10000; };
struct SpatialHit { bool hit = false; std::uint64_t id = 0; float distance = 0; };
// Ray parameter is measured along direction (metres when normalized).
bool Intersect(const Ray& ray, const Bounds& box, float& distance);
SpatialHit BruteRaycast(const std::vector<SpatialItem>& items, const Ray& ray);
class SpatialIndex {
public:
    void Build(std::vector<SpatialItem> items);
    SpatialHit Raycast(const Ray& ray) const;
    std::vector<std::uint64_t> Candidates(const Ray& ray) const;
    std::size_t Size() const { return m_items.size(); }
private:
    struct Node { Bounds box; int left = -1, right = -1; std::size_t begin = 0, count = 0; };
    int Split(std::size_t begin, std::size_t end);
    std::vector<Node> m_nodes;
    std::vector<SpatialItem> m_items;
};
}
