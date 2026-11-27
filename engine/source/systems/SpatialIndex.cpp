#include "systems/SpatialIndex.h"
#include "systems/Profiler.h"
#include <glm/common.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace eng::systems {
bool Intersect(const Ray& ray, const Bounds& box, float& distance) {
    if (!std::isfinite(ray.limit) || ray.limit < 0) return false;
    float near = 0, far = ray.limit;
    bool nonzero = false;
    for (int axis = 0; axis < 3; ++axis) {
        const float o = ray.origin[axis], d = ray.direction[axis];
        if (!std::isfinite(o) || !std::isfinite(d)) return false;
        nonzero |= d != 0;
        if (d == 0) { if (o < box.min[axis] || o > box.max[axis]) return false; continue; }
        float a = (box.min[axis] - o) / d, b = (box.max[axis] - o) / d;
        if (a > b) std::swap(a, b);
        near = std::max(near, a); far = std::min(far, b);
        if (near > far) return false;
    }
    distance = near;
    return nonzero;
}
static void Consider(SpatialHit& hit, std::uint64_t id, float distance) {
    if (!hit.hit || distance < hit.distance || (distance == hit.distance && id < hit.id))
        hit = {true, id, distance};
}
SpatialHit BruteRaycast(const std::vector<SpatialItem>& items, const Ray& ray) {
    SpatialHit result;
    for (const auto& item : items) { float t; if (Intersect(ray, item.bounds, t)) Consider(result, item.id, t); }
    return result;
}
void SpatialIndex::Build(std::vector<SpatialItem> items) {
    GD_ZONE("BVH build");
    for (const auto& item : items) for (int a = 0; a < 3; ++a)
        if (!std::isfinite(item.bounds.min[a]) || !std::isfinite(item.bounds.max[a]) || item.bounds.min[a] > item.bounds.max[a])
            throw std::invalid_argument("invalid spatial bounds");
    m_items = std::move(items); m_nodes.clear();
    if (!m_items.empty()) Split(0, m_items.size());
}
int SpatialIndex::Split(std::size_t begin, std::size_t end) {
    Node node; node.begin = begin; node.count = end - begin; node.box = m_items[begin].bounds;
    for (auto i = begin + 1; i < end; ++i) {
        node.box.min = glm::min(node.box.min, m_items[i].bounds.min);
        node.box.max = glm::max(node.box.max, m_items[i].bounds.max);
    }
    const auto index = static_cast<int>(m_nodes.size()); m_nodes.push_back(node);
    if (end - begin <= 4) return index;
    const auto extent = node.box.max - node.box.min;
    int axis = extent.y > extent.x ? 1 : 0; if (extent.z > extent[axis]) axis = 2;
    const auto mid = begin + (end - begin) / 2;
    std::nth_element(m_items.begin() + begin, m_items.begin() + mid, m_items.begin() + end,
        [axis](const auto& a, const auto& b) {
            const float ca = a.bounds.min[axis] * .5f + a.bounds.max[axis] * .5f;
            const float cb = b.bounds.min[axis] * .5f + b.bounds.max[axis] * .5f;
            return ca == cb ? a.id < b.id : ca < cb;
        });
    const int left = Split(begin, mid), right = Split(mid, end);
    m_nodes[index].left = left; m_nodes[index].right = right; m_nodes[index].count = 0;
    return index;
}
SpatialHit SpatialIndex::Raycast(const Ray& ray) const {
    GD_ZONE("BVH raycast");
    SpatialHit result;
    if (m_nodes.empty()) return result;
    std::vector<int> stack{0};
    while (!stack.empty()) {
        const auto& node = m_nodes[stack.back()]; stack.pop_back();
        Ray clipped = ray; if (result.hit) clipped.limit = result.distance;
        float t; if (!Intersect(clipped, node.box, t)) continue;
        if (!node.count) { stack.push_back(node.left); stack.push_back(node.right); continue; }
        for (auto i = node.begin; i < node.begin + node.count; ++i)
            if (Intersect(clipped, m_items[i].bounds, t)) Consider(result, m_items[i].id, t);
    }
    return result;
}
std::vector<std::uint64_t> SpatialIndex::Candidates(const Ray& ray) const {
    std::vector<std::uint64_t> result;
    if (m_nodes.empty()) return result;
    std::vector<int> stack{0};
    while (!stack.empty()) {
        const auto& node = m_nodes[stack.back()]; stack.pop_back();
        float t; if (!Intersect(ray, node.box, t)) continue;
        if (!node.count) { stack.push_back(node.left); stack.push_back(node.right); }
        else for (auto i = node.begin; i < node.begin + node.count; ++i)
            if (Intersect(ray, m_items[i].bounds, t)) result.push_back(m_items[i].id);
    }
    return result;
}
}
