#pragma once

#include "drills/DrillDefinition.h"
#include "project/ProjectPaths.h"
#include "project/SchemaVersion.h"

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace game
{
    // Documented player-feel parameters. "feelPreset" drives the simple control;
    // the individual values are the advanced controls and stay authoritative.
    struct MovementSettings
    {
        std::string feelPreset = "responsive"; // responsive | brisk | floaty | heavy | custom
        float moveSpeed = 7.0f;         // m/s, ground top speed
        float jumpHeight = 1.1f;        // m, apex above the feet
        float groundAccel = 90.0f;      // m/s^2 toward target velocity on ground
        float groundDecel = 70.0f;      // m/s^2 toward zero when no input
        float friction = 6.0f;          // 1/s exponential damping on ground
        float airControl = 0.35f;       // 0..1 fraction of ground accel while airborne
        float mouseSensitivity = 2.5f;  // degrees of yaw per mouse count unit

        nlohmann::json ToJson() const;
        static MovementSettings FromJson(const nlohmann::json& json, const MovementSettings& base = MovementSettings());
        // Named feel reference points. "responsive" is the Counter-Strike-like
        // reference (snappy, low air control) - a feel target, not a simulation.
        static MovementSettings Preset(const std::string& name);
        static std::vector<std::string> PresetNames();
    };

    struct WeaponDef
    {
        std::string id;
        std::string name;
        std::string category = "pistol";   // pistol | smg | auto | sniper
        std::string fireMode = "semi";     // semi | auto | burst
        float fireRateRpm = 450.0f;        // rounds per minute
        float damage = 34.0f;
        int magazine = 12;
        float reloadSeconds = 1.6f;
        float spreadDegrees = 1.2f;        // cone half-angle at rest
        float recoil = 1.0f;               // vertical kick multiplier
        float aimZoom = 1.0f;              // >1 narrows FOV (snipers)
        std::string modelPath;             // project-relative glTF; "" -> built-in blockout
        float modelScale = 1.0f;
        glm::vec3 gripOffset = glm::vec3(0.2f, -0.2f, -0.4f);
        glm::vec3 muzzleOffset = glm::vec3(0.0f, 0.0f, -0.6f);
        std::string fireSound;             // project-relative wav; "" -> built-in
        std::string reloadSound;
        std::string emptySound;

        float FireInterval() const { return fireRateRpm > 1.0f ? 60.0f / fireRateRpm : 0.15f; }

        nlohmann::json ToJson() const;
        static WeaponDef FromJson(const nlohmann::json& json);
        // The four starter weapons, one per category.
        static std::vector<WeaponDef> Defaults();
    };

    struct KeyBindings
    {
        // action token -> input token ("1".."4", "MouseWheelUp", "Q", ...)
        std::vector<std::pair<std::string, std::string>> entries = {
            { "weaponSlot1", "1" }, { "weaponSlot2", "2" },
            { "weaponSlot3", "3" }, { "weaponSlot4", "4" },
            { "weaponNext", "MouseWheelUp" }, { "weaponPrev", "MouseWheelDown" },
            { "reload", "R" }, { "aimDownSights", "Mouse2" },
        };

        std::string Get(const std::string& action) const;
        void Set(const std::string& action, const std::string& input);
        // actions that share an input token (conflict detection for the UI)
        std::vector<std::string> Conflicts() const;

        nlohmann::json ToJson() const;
        static KeyBindings FromJson(const nlohmann::json& json);
    };

    // One authored drill. Order in CourseProject::drills is the play order.
    struct DrillDoc
    {
        std::string id;                       // stable slug, unique in the project
        std::string name = "New Drill";
        std::string intro;
        float targetTimeSeconds = 30.0f;
        glm::vec3 playerStart = glm::vec3(0.0f, 2.0f, 8.0f);
        bool inheritMovement = true;
        MovementSettings movement;             // used only when inheritMovement == false
        std::vector<std::string> allowedWeapons; // weapon ids; empty -> every project weapon
        std::string musicPath;                 // project-relative; "" -> silence
        std::vector<ObjectiveDefinition> objectives;
        nlohmann::json content = nlohmann::json::array(); // scene-schema object list
        std::string sourceFile;                // "drills/<id>.json"

        nlohmann::json ToJson() const;
        static DrillDoc FromJson(const nlohmann::json& json);

        // The runtime definition the DrillRunner consumes. Movement is resolved
        // here so the runtime never needs to know about inheritance.
        DrillDefinition ToRuntime() const;
        MovementSettings ResolvedMovement(const MovementSettings& projectDefaults) const;
    };

    struct ProjectValidation
    {
        enum class Level { Info, Warning, Error };
        struct Issue
        {
            Level level = Level::Warning;
            std::string where;    // "Drill 2 'Speed'" / "Project"
            std::string message;
        };
        std::vector<Issue> issues;

        bool HasErrors() const;
        int Count(Level level) const;
    };

    // The whole authored course. Owns its folder; every reference inside is
    // project-relative so the folder is portable.
    class CourseProject
    {
    public:
        int schemaVersion = kProjectSchemaVersion;
        std::string name = "Untitled Course";
        std::string environmentScene = "scenes/environment.sc";
        MovementSettings movementDefaults;
        float masterVolume = 1.0f;
        float musicVolume = 0.7f;
        float effectsVolume = 0.9f;
        std::vector<WeaponDef> weapons;
        KeyBindings bindings;
        std::vector<DrillDoc> drills;

        const ProjectPaths& Paths() const { return m_paths; }
        bool IsOpen() const { return m_paths.IsValid(); }

        struct Result { bool ok = false; std::string error; };

        // Opens <root>/project.json and every drill it lists.
        static Result Load(const std::filesystem::path& projectRoot, CourseProject& out);
        // Creates a fresh project folder: project.json, standard subfolders, a
        // default environment scene, four default weapons and one starter drill.
        static Result CreateNew(const std::filesystem::path& projectRoot,
                                const std::string& displayName, CourseProject& out);

        // Writes project.json + every drill file. Returns false and fills error
        // on any filesystem failure (nothing half-written: drills go to temp
        // files first, then rename).
        bool Save(std::string* error = nullptr);
        // Copies the whole open project to a new folder and rebinds to it.
        bool SaveAs(const std::filesystem::path& newRoot, std::string* error = nullptr);
        // backups/<UTC-timestamp>.json - a single-file bundle of the whole course.
        bool WriteBackup(std::string* error = nullptr);

        ProjectValidation Validate() const;

        // ---- Drill collection ops (dynamic count; order == play order) ----
        DrillDoc& AddDrill(const std::string& displayName);
        bool RemoveDrill(std::size_t index);
        bool MoveDrill(std::size_t index, int delta);   // delta -1 up, +1 down
        DrillDoc* DuplicateDrill(std::size_t index);
        DrillDoc* FindDrill(const std::string& id);

        const WeaponDef* FindWeapon(const std::string& id) const;

        nlohmann::json ToProjectJson() const;   // project.json contents (no drills inlined)

    private:
        void ReadProjectJson(const nlohmann::json& json);
        std::string MakeUniqueDrillId(const std::string& base) const;

        ProjectPaths m_paths;
    };
}
