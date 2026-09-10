#include "project/ProjectPaths.h"

#include <system_error>

namespace game
{
    ProjectPaths::ProjectPaths(std::filesystem::path root)
        : m_root(std::move(root))
    {
        if (!m_root.empty())
        {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(m_root, ec);
            if (!ec)
            {
                m_root = canonical;
            }
        }
    }

    std::filesystem::path ProjectPaths::ProjectFile() const { return m_root / "project.json"; }
    std::filesystem::path ProjectPaths::DrillsDir() const { return m_root / "drills"; }
    std::filesystem::path ProjectPaths::ScenesDir() const { return m_root / "scenes"; }
    std::filesystem::path ProjectPaths::AssetsDir() const { return m_root / "assets"; }
    std::filesystem::path ProjectPaths::BackupsDir() const { return m_root / "backups"; }
    std::filesystem::path ProjectPaths::LayoutFile() const { return m_root / ".layout.ini"; }

    std::filesystem::path ProjectPaths::Resolve(const std::string& relative) const
    {
        std::filesystem::path rel(relative);
        if (rel.is_absolute())
        {
            return rel;
        }
        return m_root / rel;
    }

    std::string ProjectPaths::MakeRelative(const std::filesystem::path& absolute) const
    {
        std::error_code ec;
        auto rel = std::filesystem::relative(absolute, m_root, ec);
        const std::string relStr = rel.generic_string();
        if (ec || relStr.empty() || relStr.rfind("..", 0) == 0)
        {
            return absolute.generic_string();
        }
        return relStr;
    }

    bool ProjectPaths::EnsureLayout() const
    {
        std::error_code ec;
        std::filesystem::create_directories(DrillsDir(), ec);
        std::filesystem::create_directories(ScenesDir(), ec);
        std::filesystem::create_directories(AssetsDir(), ec);
        std::filesystem::create_directories(BackupsDir(), ec);
        return !ec;
    }
}
