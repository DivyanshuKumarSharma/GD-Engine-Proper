#pragma once
#include "systems/JobSystem.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace eng::systems {
// Independent byte reads capture absolute paths, never a mutable project root.
// Consumer polls its future and uploads/instantiates on the main thread.
inline std::future<std::vector<unsigned char>> StreamAsset(
    JobSystem& jobs, std::filesystem::path path, std::size_t maxBytes = 64 * 1024 * 1024) {
    return jobs.Submit([path = std::move(path), maxBytes] {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("cannot open asset: " + path.string());
        const auto size = file.tellg();
        if (size < 0 || static_cast<std::uintmax_t>(size) > maxBytes)
            throw std::runtime_error("asset exceeds read limit: " + path.string());
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), size))
            throw std::runtime_error("incomplete asset read: " + path.string());
        return bytes;
    });
}
}
