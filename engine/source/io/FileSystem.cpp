#include "io/FileSystem.h"

#include "config.h"

#if defined _WIN32
#include <windows.h>
#elif defined (__APPLE__)
#include <mach-o/dyld.h>
#elif defined (__linux__)
#include <unistd.h>
#include <limits.h>
#endif

#include <fstream>

namespace eng
{
    std::filesystem::path FileSystem::GetExecutableFolder() const
    {
#if defined _WIN32
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(NULL, buf, MAX_PATH);
        return std::filesystem::path(buf).remove_filename();
#elif defined (__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string tmp(size, '\0');
        _NSGetExecutablePath(tmp.data(), &size);
        return std::filesystem::weakly_canonical(std::filesystem::path(tmp)).remove_filename();
#elif defined (__linux__)
        return std::filesystem::weakly_canonical(std::filesystem::read_symlink("/proc/self/exe")).remove_filename();
#else
        return std::filesystem::current_path();
#endif
    }

    std::filesystem::path FileSystem::GetAssetsFolder() const
    {
        if (!m_assetsRootOverride.empty())
        {
            return m_assetsRootOverride;
        }
        return GetDefaultAssetsFolder();
    }

    std::filesystem::path FileSystem::GetDefaultAssetsFolder() const
    {
        // Prefer packaged assets; a development checkout must never silently
        // override the contents of a downloaded, relocated playable build.
        const auto executable = GetExecutableFolder();
        if (std::filesystem::is_directory(executable / "assets")) return executable / "assets";
        if (std::filesystem::is_directory(executable.parent_path() / "assets")) return executable.parent_path() / "assets";
#if defined (ASSETS_ROOT)
        auto path = std::filesystem::path(std::string(ASSETS_ROOT));
        if (std::filesystem::exists(path))
        {
            return path;
        }
#endif
        return std::filesystem::weakly_canonical(GetExecutableFolder() / "assets");
    }

    void FileSystem::SetAssetsRootOverride(const std::filesystem::path& root)
    {
        if (root.empty())
        {
            m_assetsRootOverride.clear();
            return;
        }
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(root, ec);
        m_assetsRootOverride = ec ? root : canonical;
    }

    const std::filesystem::path& FileSystem::GetAssetsRootOverride() const
    {
        return m_assetsRootOverride;
    }

    std::vector<char> FileSystem::LoadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return {};
        }

        auto size = file.tellg();
        file.seekg(0);

        std::vector<char> buffer(size);

        if (!file.read(buffer.data(), size))
        {
            return {};
        }

        return buffer;
    }

    std::vector<char> FileSystem::LoadAssetFile(const std::string& relativePath)
    {
        return LoadFile(GetAssetsFolder() / relativePath);
    }

    std::string FileSystem::LoadAssetFileText(const std::string& relativePath)
    {
        auto buffer = LoadAssetFile(relativePath);
        return std::string(buffer.begin(), buffer.end());
    }

    bool FileSystem::SaveAssetFileText(const std::string& relativePath, const std::string& contents)
    {
        return SaveTextFile(GetAssetsFolder() / relativePath, contents);
    }

    std::string FileSystem::LoadTextFile(const std::filesystem::path& path)
    {
        auto buffer = LoadFile(path);
        return std::string(buffer.begin(), buffer.end());
    }

    bool FileSystem::SaveTextFile(const std::filesystem::path& path, const std::string& contents)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            return false;
        }

        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return file.good();
    }
}
