#pragma once

#include <filesystem>
#include <string>

namespace game
{
    // Resolves the well-known locations inside a portable course-project folder.
    // Every reference a project stores is relative to the project root, so the
    // whole folder can be zipped, moved or shared and still opens.
    //
    //   <root>/project.json          - the course document
    //   <root>/drills/*.json         - one file per drill (ordered by project.json)
    //   <root>/scenes/*.sc           - the shared environment shell
    //   <root>/assets/               - imported models / images / audio (copied in)
    //   <root>/backups/              - timestamped autosaves
    //   <root>/.layout.ini           - editor dock layout (per project)
    class ProjectPaths
    {
    public:
        ProjectPaths() = default;
        explicit ProjectPaths(std::filesystem::path root);

        const std::filesystem::path& Root() const { return m_root; }
        bool IsValid() const { return !m_root.empty(); }

        std::filesystem::path ProjectFile() const;   // <root>/project.json
        std::filesystem::path DrillsDir() const;
        std::filesystem::path ScenesDir() const;
        std::filesystem::path AssetsDir() const;
        std::filesystem::path BackupsDir() const;
        std::filesystem::path LayoutFile() const;

        // Absolute path for a project-relative reference (e.g. "assets/rig.glb").
        std::filesystem::path Resolve(const std::string& relative) const;
        // Project-relative form of an absolute path, or the input unchanged when
        // it is not inside the project.
        std::string MakeRelative(const std::filesystem::path& absolute) const;

        // Creates the standard subfolders. Returns false on a filesystem error.
        bool EnsureLayout() const;

    private:
        std::filesystem::path m_root;
    };
}
