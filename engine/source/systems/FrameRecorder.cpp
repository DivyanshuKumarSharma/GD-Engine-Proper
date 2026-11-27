#include "systems/FrameRecorder.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace eng::systems {
void FrameRecorder::Write(const std::filesystem::path& path) const {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write frame capture: " + path.string());
    out << "frame,frame_ms\n" << std::setprecision(10);
    for (std::size_t i = 0; i < m_frames.size(); ++i) out << i << ',' << m_frames[i] << '\n';
    if (!out) throw std::runtime_error("failed writing frame capture");
}
}
