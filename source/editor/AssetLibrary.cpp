#include "editor/AssetLibrary.h"

#include "editor/EditorLog.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace editor
{
    namespace
    {
        std::string ToLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }
    }

    AssetType AssetLibrary::ClassifyExtension(const std::string& ext)
    {
        if (ext == ".gltf" || ext == ".glb") return AssetType::Model;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") return AssetType::Image;
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") return AssetType::Audio;
        if (ext == ".mat") return AssetType::Material;
        if (ext == ".sc") return AssetType::Scene;
        if (ext == ".ttf" || ext == ".otf") return AssetType::Font;
        return AssetType::Other;
    }

    const char* AssetLibrary::TypeLabel(AssetType type)
    {
        switch (type)
        {
        case AssetType::Model: return "Model";
        case AssetType::Image: return "Image";
        case AssetType::Audio: return "Audio";
        case AssetType::Material: return "Material";
        case AssetType::Scene: return "Scene";
        case AssetType::Font: return "Font";
        default: return "File";
        }
    }

    void AssetLibrary::Rescan(const std::filesystem::path& projectRoot)
    {
        m_root = projectRoot;
        m_entries.clear();

        std::error_code ec;
        if (!std::filesystem::exists(projectRoot, ec))
        {
            return;
        }

        for (auto it = std::filesystem::recursive_directory_iterator(
                 projectRoot, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                break;
            }
            const auto& path = it->path();

            // Skip editor bookkeeping folders.
            const std::filesystem::path rel = std::filesystem::relative(path, projectRoot, ec);
            const std::string top = (!ec && !rel.empty()) ? rel.begin()->string() : std::string();
            if (top == "backups" || top == ".git" || (!top.empty() && top[0] == '.'))
            {
                if (it->is_directory())
                {
                    it.disable_recursion_pending();
                }
                continue;
            }
            if (!it->is_regular_file(ec))
            {
                continue;
            }

            const AssetType type = ClassifyExtension(ToLower(path.extension().string()));
            if (type == AssetType::Other)
            {
                continue;
            }

            AssetEntry entry;
            entry.type = type;
            entry.relativePath = std::filesystem::relative(path, projectRoot, ec).generic_string();
            entry.name = path.filename().string();
            entry.sizeBytes = std::filesystem::file_size(path, ec);
            m_entries.push_back(std::move(entry));
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const AssetEntry& a, const AssetEntry& b)
        {
            if (a.type != b.type) return a.type < b.type;
            return a.relativePath < b.relativePath;
        });

        EditorLog::Info("assets", "Scanned " + std::to_string(m_entries.size()) +
            " asset(s) in " + projectRoot.string());
    }

    std::vector<const AssetEntry*> AssetLibrary::Query(const std::string& search, AssetType type) const
    {
        const std::string needle = ToLower(search);
        std::vector<const AssetEntry*> out;
        for (const auto& entry : m_entries)
        {
            if (type != AssetType::Other && entry.type != type)
            {
                continue;
            }
            if (!needle.empty() && ToLower(entry.relativePath).find(needle) == std::string::npos)
            {
                continue;
            }
            out.push_back(&entry);
        }
        return out;
    }

    const AssetEntry* AssetLibrary::Find(const std::string& relativePath) const
    {
        for (const auto& entry : m_entries)
        {
            if (entry.relativePath == relativePath)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    void AssetLibrary::NoteReference(const std::string& relativePath)
    {
        if (relativePath.empty() || Find(relativePath))
        {
            return;
        }
        std::error_code ec;
        const bool exists = !m_root.empty() && std::filesystem::exists(m_root / relativePath, ec);

        AssetEntry entry;
        entry.relativePath = relativePath;
        entry.name = std::filesystem::path(relativePath).filename().string();
        entry.type = ClassifyExtension(ToLower(std::filesystem::path(relativePath).extension().string()));
        entry.missing = !exists;
        m_entries.push_back(std::move(entry));

        if (!exists)
        {
            EditorLog::Warn("assets", "Referenced asset is missing: " + relativePath);
        }
    }
}
