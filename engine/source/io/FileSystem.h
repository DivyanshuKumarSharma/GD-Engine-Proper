#pragma once
#include <filesystem>
#include <vector>

namespace eng
{
    class FileSystem
    {
    public:
        std::filesystem::path GetExecutableFolder() const;
        std::filesystem::path GetAssetsFolder() const;

        // Redirects every asset-relative load/save at a different root. The
        // editor points this at the open project folder so a portable project
        // (its own scenes/, models/, audio/ ...) resolves without touching the
        // built-in assets. Pass an empty path to restore the default root.
        void SetAssetsRootOverride(const std::filesystem::path& root);
        const std::filesystem::path& GetAssetsRootOverride() const;

        std::vector<char> LoadFile(const std::filesystem::path& path);
        std::vector<char> LoadAssetFile(const std::string& relativePath);
        std::string LoadAssetFileText(const std::string& relativePath);
        bool SaveAssetFileText(const std::string& relativePath, const std::string& contents);

        // Generic (non asset-relative) text IO, e.g. for save data next to the
        // executable. Returns an empty string when the file is missing.
        std::string LoadTextFile(const std::filesystem::path& path);
        bool SaveTextFile(const std::filesystem::path& path, const std::string& contents);

    private:
        std::filesystem::path m_assetsRootOverride;
    };
}
