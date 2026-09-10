#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor
{
    enum class AssetType
    {
        Model,
        Image,
        Audio,
        Material,
        Scene,
        Font,
        Other
    };

    struct AssetEntry
    {
        AssetType type = AssetType::Other;
        std::string relativePath;   // project-relative, forward slashes
        std::string name;           // filename without directory
        std::uintmax_t sizeBytes = 0;
        bool missing = false;       // referenced but not on disk
    };

    // Scans a project folder for usable assets and backs the asset browser.
    // Purely a catalogue - it never loads GPU resources.
    class AssetLibrary
    {
    public:
        void Rescan(const std::filesystem::path& projectRoot);
        const std::vector<AssetEntry>& All() const { return m_entries; }

        // Case-insensitive substring match on name/path, optionally filtered by
        // type (AssetType::Other == "any").
        std::vector<const AssetEntry*> Query(const std::string& search, AssetType type) const;

        const AssetEntry* Find(const std::string& relativePath) const;

        // Records that `relativePath` is referenced; if it is not on disk it is
        // added as a "missing" entry so the browser can flag it.
        void NoteReference(const std::string& relativePath);

        static AssetType ClassifyExtension(const std::string& extLowerWithDot);
        static const char* TypeLabel(AssetType type);

    private:
        std::filesystem::path m_root;
        std::vector<AssetEntry> m_entries;
    };
}
