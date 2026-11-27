#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>

namespace eng::systems {
// CPU wall time including swap/present. Does not claim GPU timing.
class FrameRecorder {
public:
    void Add(double milliseconds) { if (m_frames.size() < 1000000) m_frames.push_back(milliseconds); }
    void Write(const std::filesystem::path& path) const;
    std::size_t Size() const { return m_frames.size(); }
private:
    std::vector<double> m_frames;
};
}
