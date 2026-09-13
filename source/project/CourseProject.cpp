#include "project/CourseProject.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

namespace game
{
    namespace
    {
        nlohmann::json Vec3Json(const glm::vec3& v)
        {
            return { { "x", v.x }, { "y", v.y }, { "z", v.z } };
        }

        glm::vec3 ReadVec3(const nlohmann::json& parent, const char* key, const glm::vec3& fallback)
        {
            if (!parent.contains(key) || !parent[key].is_object())
            {
                return fallback;
            }
            const auto& v = parent[key];
            return glm::vec3(v.value("x", fallback.x), v.value("y", fallback.y), v.value("z", fallback.z));
        }

        std::string ReadTextFile(const std::filesystem::path& path, bool* ok)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
            {
                if (ok) *ok = false;
                return {};
            }
            std::ostringstream stream;
            stream << file.rdbuf();
            if (ok) *ok = true;
            return stream.str();
        }

        bool WriteTextFileAtomic(const std::filesystem::path& path, const std::string& contents, std::string* error)
        {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);

            const std::filesystem::path temp = path.string() + ".tmp";
            {
                std::ofstream file(temp, std::ios::binary | std::ios::trunc);
                if (!file.is_open())
                {
                    if (error) *error = "cannot open for write: " + temp.string();
                    return false;
                }
                file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
                if (!file.good())
                {
                    if (error) *error = "write failed: " + temp.string();
                    return false;
                }
            }
            std::filesystem::rename(temp, path, ec);
            if (ec)
            {
                // Some filesystems refuse rename-over; fall back to remove+rename.
                std::filesystem::remove(path, ec);
                std::filesystem::rename(temp, path, ec);
                if (ec)
                {
                    if (error) *error = "atomic replace failed: " + path.string();
                    return false;
                }
            }
            return true;
        }

        std::string Slugify(const std::string& text)
        {
            std::string out;
            bool lastDash = false;
            for (char c : text)
            {
                if (std::isalnum(static_cast<unsigned char>(c)))
                {
                    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    lastDash = false;
                }
                else if (!lastDash && !out.empty())
                {
                    out += '-';
                    lastDash = true;
                }
            }
            while (!out.empty() && out.back() == '-')
            {
                out.pop_back();
            }
            return out.empty() ? "drill" : out;
        }

        std::string TimestampUtc()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#if defined(_WIN32)
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &tm);
            return buffer;
        }
    }

    // ----------------------------------------------------------------- Movement
    nlohmann::json MovementSettings::ToJson() const
    {
        return {
            { "feelPreset", feelPreset },
            { "moveSpeed", moveSpeed },
            { "jumpHeight", jumpHeight },
            { "groundAccel", groundAccel },
            { "groundDecel", groundDecel },
            { "friction", friction },
            { "airControl", airControl },
            { "mouseSensitivity", mouseSensitivity },
        };
    }

    MovementSettings MovementSettings::FromJson(const nlohmann::json& json, const MovementSettings& base)
    {
        MovementSettings m = base;
        if (!json.is_object())
        {
            return m;
        }
        m.feelPreset = json.value("feelPreset", m.feelPreset);
        m.moveSpeed = json.value("moveSpeed", m.moveSpeed);
        m.jumpHeight = json.value("jumpHeight", m.jumpHeight);
        m.groundAccel = json.value("groundAccel", m.groundAccel);
        m.groundDecel = json.value("groundDecel", m.groundDecel);
        m.friction = json.value("friction", m.friction);
        m.airControl = json.value("airControl", m.airControl);
        m.mouseSensitivity = json.value("mouseSensitivity", m.mouseSensitivity);
        return m;
    }

    MovementSettings MovementSettings::Preset(const std::string& name)
    {
        MovementSettings m;
        m.feelPreset = name;
        if (name == "responsive")
        {
            m.moveSpeed = 8.0f; m.jumpHeight = 1.1f;
            m.groundAccel = 110.0f; m.groundDecel = 90.0f;
            m.friction = 7.0f; m.airControl = 0.30f; m.mouseSensitivity = 2.5f;
        }
        else if (name == "brisk")
        {
            m.moveSpeed = 6.5f; m.jumpHeight = 1.0f;
            m.groundAccel = 80.0f; m.groundDecel = 65.0f;
            m.friction = 6.0f; m.airControl = 0.4f; m.mouseSensitivity = 2.5f;
        }
        else if (name == "floaty")
        {
            m.moveSpeed = 6.0f; m.jumpHeight = 1.6f;
            m.groundAccel = 45.0f; m.groundDecel = 30.0f;
            m.friction = 3.0f; m.airControl = 0.7f; m.mouseSensitivity = 2.5f;
        }
        else if (name == "heavy")
        {
            m.moveSpeed = 5.0f; m.jumpHeight = 0.8f;
            m.groundAccel = 35.0f; m.groundDecel = 40.0f;
            m.friction = 8.0f; m.airControl = 0.15f; m.mouseSensitivity = 2.2f;
        }
        // "custom" -> struct defaults
        return m;
    }

    std::vector<std::string> MovementSettings::PresetNames()
    {
        return { "responsive", "brisk", "floaty", "heavy", "custom" };
    }

    // ------------------------------------------------------------------- Weapon
    nlohmann::json WeaponDef::ToJson() const
    {
        return {
            { "id", id }, { "name", name }, { "category", category },
            { "fireMode", fireMode }, { "fireRateRpm", fireRateRpm },
            { "damage", damage }, { "magazine", magazine },
            { "reloadSeconds", reloadSeconds }, { "spreadDegrees", spreadDegrees },
            { "recoil", recoil }, { "aimZoom", aimZoom },
            { "modelPath", modelPath }, { "modelScale", modelScale },
            { "gripOffset", Vec3Json(gripOffset) }, { "muzzleOffset", Vec3Json(muzzleOffset) },
            { "fireSound", fireSound }, { "reloadSound", reloadSound }, { "emptySound", emptySound },
        };
    }

    WeaponDef WeaponDef::FromJson(const nlohmann::json& json)
    {
        WeaponDef w;
        w.id = json.value("id", "");
        w.name = json.value("name", w.id);
        w.category = json.value("category", w.category);
        w.fireMode = json.value("fireMode", w.fireMode);
        w.fireRateRpm = json.value("fireRateRpm", w.fireRateRpm);
        w.damage = json.value("damage", w.damage);
        w.magazine = json.value("magazine", w.magazine);
        w.reloadSeconds = json.value("reloadSeconds", w.reloadSeconds);
        w.spreadDegrees = json.value("spreadDegrees", w.spreadDegrees);
        w.recoil = json.value("recoil", w.recoil);
        w.aimZoom = json.value("aimZoom", w.aimZoom);
        w.modelPath = json.value("modelPath", "");
        w.modelScale = json.value("modelScale", w.modelScale);
        w.gripOffset = ReadVec3(json, "gripOffset", w.gripOffset);
        w.muzzleOffset = ReadVec3(json, "muzzleOffset", w.muzzleOffset);
        w.fireSound = json.value("fireSound", "");
        w.reloadSound = json.value("reloadSound", "");
        w.emptySound = json.value("emptySound", "");
        return w;
    }

    std::vector<WeaponDef> WeaponDef::Defaults()
    {
        std::vector<WeaponDef> list;

        WeaponDef pistol;
        pistol.id = "sidearm-9"; pistol.name = "Sidearm 9"; pistol.category = "pistol";
        pistol.fireMode = "semi"; pistol.fireRateRpm = 360.0f; pistol.damage = 34.0f;
        pistol.magazine = 15; pistol.reloadSeconds = 1.4f; pistol.spreadDegrees = 0.8f;
        pistol.recoil = 0.8f;
        list.push_back(pistol);

        WeaponDef smg;
        smg.id = "compact-smg"; smg.name = "Compact SMG"; smg.category = "smg";
        smg.fireMode = "auto"; smg.fireRateRpm = 850.0f; smg.damage = 18.0f;
        smg.magazine = 30; smg.reloadSeconds = 1.8f; smg.spreadDegrees = 1.8f;
        smg.recoil = 0.9f;
        list.push_back(smg);

        WeaponDef rifle;
        rifle.id = "service-rifle"; rifle.name = "Service Rifle"; rifle.category = "auto";
        rifle.fireMode = "auto"; rifle.fireRateRpm = 650.0f; rifle.damage = 28.0f;
        rifle.magazine = 25; rifle.reloadSeconds = 2.2f; rifle.spreadDegrees = 1.2f;
        rifle.recoil = 1.2f;
        list.push_back(rifle);

        WeaponDef sniper;
        sniper.id = "bolt-sniper"; sniper.name = "Bolt Sniper"; sniper.category = "sniper";
        sniper.fireMode = "semi"; sniper.fireRateRpm = 45.0f; sniper.damage = 120.0f;
        sniper.magazine = 5; sniper.reloadSeconds = 3.0f; sniper.spreadDegrees = 0.1f;
        sniper.recoil = 2.5f; sniper.aimZoom = 4.0f;
        list.push_back(sniper);

        return list;
    }

    // --------------------------------------------------------------- KeyBindings
    std::string KeyBindings::Get(const std::string& action) const
    {
        for (const auto& e : entries)
        {
            if (e.first == action) return e.second;
        }
        return {};
    }

    void KeyBindings::Set(const std::string& action, const std::string& input)
    {
        for (auto& e : entries)
        {
            if (e.first == action) { e.second = input; return; }
        }
        entries.emplace_back(action, input);
    }

    std::vector<std::string> KeyBindings::Conflicts() const
    {
        std::vector<std::string> conflicting;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            for (std::size_t j = i + 1; j < entries.size(); ++j)
            {
                if (!entries[i].second.empty() && entries[i].second == entries[j].second)
                {
                    if (std::find(conflicting.begin(), conflicting.end(), entries[i].first) == conflicting.end())
                        conflicting.push_back(entries[i].first);
                    if (std::find(conflicting.begin(), conflicting.end(), entries[j].first) == conflicting.end())
                        conflicting.push_back(entries[j].first);
                }
            }
        }
        return conflicting;
    }

    nlohmann::json KeyBindings::ToJson() const
    {
        nlohmann::json json = nlohmann::json::object();
        for (const auto& e : entries)
        {
            json[e.first] = e.second;
        }
        return json;
    }

    KeyBindings KeyBindings::FromJson(const nlohmann::json& json)
    {
        KeyBindings b; // starts from the default set; JSON overrides by action
        if (json.is_object())
        {
            for (auto it = json.begin(); it != json.end(); ++it)
            {
                if (it.value().is_string())
                {
                    b.Set(it.key(), it.value().get<std::string>());
                }
            }
        }
        return b;
    }

    // ------------------------------------------------------------------ DrillDoc
    nlohmann::json DrillDoc::ToJson() const
    {
        nlohmann::json objectivesJson = nlohmann::json::array();
        for (const auto& o : objectives)
        {
            objectivesJson.push_back({
                { "type", o.type },
                { "description", o.description },
                { "amount", o.amount },
                { "targetKind", o.targetKind },
                { "tag", o.tag },
            });
        }

        nlohmann::json json = {
            { "schemaVersion", kDrillSchemaVersion },
            { "id", id },
            { "name", name },
            { "intro", intro },
            { "targetTime", targetTimeSeconds },
            { "playerStart", Vec3Json(playerStart) },
            { "inheritMovement", inheritMovement },
            { "allowedWeapons", allowedWeapons },
            { "music", musicPath },
            { "objectives", objectivesJson },
            { "objects", content },
        };
        if (!inheritMovement)
        {
            json["movement"] = movement.ToJson();
        }
        return json;
    }

    DrillDoc DrillDoc::FromJson(const nlohmann::json& json)
    {
        DrillDoc d;
        d.id = json.value("id", "");
        d.name = json.value("name", d.id.empty() ? "Drill" : d.id);
        d.intro = json.value("intro", "");
        d.targetTimeSeconds = json.value("targetTime", d.targetTimeSeconds);
        d.playerStart = ReadVec3(json, "playerStart", d.playerStart);
        d.inheritMovement = json.value("inheritMovement", true);
        if (json.contains("movement"))
        {
            d.movement = MovementSettings::FromJson(json["movement"]);
        }
        if (json.contains("allowedWeapons") && json["allowedWeapons"].is_array())
        {
            for (const auto& w : json["allowedWeapons"])
            {
                d.allowedWeapons.push_back(w.get<std::string>());
            }
        }
        d.musicPath = json.value("music", "");
        if (json.contains("objectives") && json["objectives"].is_array())
        {
            for (const auto& e : json["objectives"])
            {
                ObjectiveDefinition o;
                o.type = e.value("type", "");
                o.description = e.value("description", "");
                o.amount = e.value("amount", 1);
                o.targetKind = e.value("targetKind", "");
                o.tag = e.value("tag", "");
                d.objectives.push_back(std::move(o));
            }
        }
        if (json.contains("objects") && json["objects"].is_array())
        {
            d.content = json["objects"];
        }
        return d;
    }

    MovementSettings DrillDoc::ResolvedMovement(const MovementSettings& projectDefaults) const
    {
        return inheritMovement ? projectDefaults : movement;
    }

    DrillDefinition DrillDoc::ToRuntime() const
    {
        DrillDefinition def;
        def.id = id;
        def.name = name;
        def.intro = intro;
        def.targetTimeSeconds = targetTimeSeconds;
        def.playerStart = playerStart;
        def.objectives = objectives;
        def.content = content;
        return def;
    }

    // -------------------------------------------------------------- CourseProject
    nlohmann::json CourseProject::ToProjectJson() const
    {
        nlohmann::json weaponsJson = nlohmann::json::array();
        for (const auto& w : weapons)
        {
            weaponsJson.push_back(w.ToJson());
        }
        nlohmann::json drillsJson = nlohmann::json::array();
        for (const auto& d : drills)
        {
            drillsJson.push_back(d.sourceFile.empty() ? ("drills/" + d.id + ".json") : d.sourceFile);
        }

        return {
            { "schemaVersion", schemaVersion },
            { "name", name },
            { "environment", environmentScene },
            { "movementDefaults", movementDefaults.ToJson() },
            { "audio", { { "master", masterVolume }, { "music", musicVolume }, { "effects", effectsVolume } } },
            { "weapons", weaponsJson },
            { "bindings", bindings.ToJson() },
            { "drills", drillsJson },
        };
    }

    void CourseProject::ReadProjectJson(const nlohmann::json& json)
    {
        schemaVersion = json.value("schemaVersion", 1);
        name = json.value("name", "Untitled Course");
        environmentScene = json.value("environment", "scenes/environment.sc");
        if (json.contains("movementDefaults"))
        {
            movementDefaults = MovementSettings::FromJson(json["movementDefaults"]);
        }
        if (json.contains("audio"))
        {
            const auto& a = json["audio"];
            masterVolume = a.value("master", masterVolume);
            musicVolume = a.value("music", musicVolume);
            effectsVolume = a.value("effects", effectsVolume);
        }
        weapons.clear();
        if (json.contains("weapons") && json["weapons"].is_array())
        {
            for (const auto& w : json["weapons"])
            {
                weapons.push_back(WeaponDef::FromJson(w));
            }
        }
        if (weapons.empty())
        {
            weapons = WeaponDef::Defaults();
        }
        if (json.contains("bindings"))
        {
            bindings = KeyBindings::FromJson(json["bindings"]);
        }
    }

    CourseProject::Result CourseProject::Load(const std::filesystem::path& projectRoot, CourseProject& out)
    {
        CourseProject project;
        project.m_paths = ProjectPaths(projectRoot);

        bool ok = false;
        const std::string text = ReadTextFile(project.m_paths.ProjectFile(), &ok);
        if (!ok)
        {
            return { false, "project.json not found in " + projectRoot.string() };
        }

        nlohmann::json json;
        try
        {
            json = nlohmann::json::parse(text);
        }
        catch (const std::exception& e)
        {
            return { false, std::string("project.json is not valid JSON: ") + e.what() };
        }

        const int fileVersion = json.value("schemaVersion", 1);
        if (fileVersion > kProjectSchemaVersion)
        {
            return { false, "project was saved by a newer CourseMaker (schema v" +
                     std::to_string(fileVersion) + ", this build understands v" +
                     std::to_string(kProjectSchemaVersion) + ")" };
        }

        try
        {
            project.ReadProjectJson(json);
        }
        catch (const std::exception& e)
        {
            return { false, std::string("project.json is malformed: ") + e.what() };
        }

        project.drills.clear();
        if (json.contains("drills") && json["drills"].is_array())
        {
            for (const auto& entry : json["drills"])
            {
                const std::string rel = entry.get<std::string>();
                bool drillOk = false;
                const std::string drillText = ReadTextFile(project.m_paths.Resolve(rel), &drillOk);
                if (!drillOk)
                {
                    return { false, "drill file missing: " + rel };
                }
                try
                {
                    DrillDoc doc = DrillDoc::FromJson(nlohmann::json::parse(drillText));
                    doc.sourceFile = rel;
                    if (doc.id.empty())
                    {
                        doc.id = Slugify(doc.name);
                    }
                    project.drills.push_back(std::move(doc));
                }
                catch (const std::exception& e)
                {
                    return { false, "drill '" + rel + "' is malformed: " + e.what() };
                }
            }
        }

        out = std::move(project);
        return { true, {} };
    }

    CourseProject::Result CourseProject::CreateNew(const std::filesystem::path& projectRoot,
                                                  const std::string& displayName, CourseProject& out)
    {
        std::error_code ec;
        std::filesystem::create_directories(projectRoot, ec);
        if (ec)
        {
            return { false, "cannot create project folder: " + projectRoot.string() };
        }

        CourseProject project;
        project.m_paths = ProjectPaths(projectRoot);
        project.m_paths.EnsureLayout();
        project.name = displayName.empty() ? "Untitled Course" : displayName;
        project.weapons = WeaponDef::Defaults();
        project.movementDefaults = MovementSettings::Preset("responsive");

        // Minimal shared environment shell: a floor, four low walls and a light.
        const auto box = [](const char* name, glm::vec3 pos, glm::vec3 size, glm::vec3 colour, bool collide)
        {
            nlohmann::json comps = nlohmann::json::array();
            comps.push_back({
                { "type", "MeshComponent" },
                { "mesh", { { "type", "box" }, { "x", size.x }, { "y", size.y }, { "z", size.z } } },
                { "material", { { "path", "materials/checker.mat" },
                    { "params", { { "float3", nlohmann::json::array({
                        { { "name", "color" }, { "value0", colour.x }, { "value1", colour.y }, { "value2", colour.z } } }) } } } } },
            });
            if (collide)
            {
                comps.push_back({
                    { "type", "PhysicsComponent" },
                    { "collider", { { "type", "box" }, { "x", size.x }, { "y", size.y }, { "z", size.z } } },
                    { "body", { { "mass", 0 }, { "friction", 0.7 }, { "type", "static" } } },
                });
            }
            return nlohmann::json{
                { "name", name },
                { "position", Vec3Json(pos) },
                { "components", comps },
            };
        };

        // A minimal playable spawn: CameraComponent + PlayerControllerComponent
        // (its accel/decel/friction/etc. are baked from MovementSettings at Play
        // time - see EditorApp::PatchPlayerMovement) + audio. No weapon child:
        // Play equips one from the project's WeaponDefs automatically.
        nlohmann::json mainPlayer = {
            { "name", "MainPlayer" },
            { "type", "Player" },
            { "config", "config/player.json" },
            { "position", Vec3Json({ 0, 2, 10 }) },
            { "components", nlohmann::json::array({
                { { "type", "CameraComponent" } },
                { { "type", "PlayerControllerComponent" },
                  { "moveSpeed", 8.0 }, { "lookSensitivity", 15.0 }, { "jumpSpeed", 5.0 },
                  { "collider", { { "radius", 0.4 }, { "height", 1.2 } } },
                  { "actions", { { "forward", "MoveForward" }, { "backward", "MoveBackward" },
                                 { "left", "MoveLeft" }, { "right", "MoveRight" }, { "jump", "Jump" } } } },
                { { "type", "AudioListenerComponent" } },
                { { "type", "AudioComponent" },
                  { "audio", nlohmann::json::array({
                      { { "name", "shoot" }, { "path", "audio/shoot.wav" } },
                      { { "name", "step" }, { "path", "audio/step.wav" } },
                      { { "name", "jump" }, { "path", "audio/jump.wav" } } }) } },
            }) },
        };

        nlohmann::json root = {
            { "name", "Environment" },
            { "children", nlohmann::json::array({
                mainPlayer,
                box("Ground", { 0, 0, 0 }, { 40, 1, 40 }, { 0.62f, 0.64f, 0.68f }, true),
                box("WallNorth", { 0, 3, -20.5f }, { 41, 6, 1 }, { 0.5f, 0.53f, 0.58f }, true),
                box("WallSouth", { 0, 3, 20.5f }, { 41, 6, 1 }, { 0.5f, 0.53f, 0.58f }, true),
                box("WallWest", { -20.5f, 3, 0 }, { 1, 6, 40 }, { 0.5f, 0.53f, 0.58f }, true),
                box("WallEast", { 20.5f, 3, 0 }, { 1, 6, 40 }, { 0.5f, 0.53f, 0.58f }, true),
                { { "name", "KeyLight" }, { "position", Vec3Json({ 8, 18, 12 }) },
                  { "components", nlohmann::json::array({
                      { { "type", "LightComponent" }, { "color", { { "r", 1.0 }, { "g", 0.98 }, { "b", 0.94 } } } } }) } },
            }) },
        };
        nlohmann::json environment = {
            { "name", project.name },
            { "camera", "MainPlayer" },
            { "objects", nlohmann::json::array({ root }) },
        };

        std::string error;
        if (!WriteTextFileAtomic(project.m_paths.Resolve(project.environmentScene), environment.dump(2), &error))
        {
            return { false, error };
        }

        // Default action bindings (movement/fire/pause/confirm/aim) so a brand
        // new project plays immediately, without needing to hand-author input.json.
        nlohmann::json inputMap = {
            { "actions", {
                { "MoveForward", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "W" } }, { { "device", "keyboard" }, { "code", "Up" } } }) },
                { "MoveBackward", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "S" } }, { { "device", "keyboard" }, { "code", "Down" } } }) },
                { "MoveLeft", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "A" } }, { { "device", "keyboard" }, { "code", "Left" } } }) },
                { "MoveRight", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "D" } }, { { "device", "keyboard" }, { "code", "Right" } } }) },
                { "Jump", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "Space" } } }) },
                { "Fire", nlohmann::json::array({ { { "device", "mouse" }, { "code", "Left" } } }) },
                { "AimDownSights", nlohmann::json::array({ { { "device", "mouse" }, { "code", "Right" } } }) },
                { "Pause", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "Escape" } } }) },
                { "Confirm", nlohmann::json::array({ { { "device", "keyboard" }, { "code", "Enter" } } }) },
            } },
        };
        if (!WriteTextFileAtomic(project.m_paths.Resolve("config/input.json"), inputMap.dump(2), &error))
        {
            return { false, error };
        }

        // Base weapon behaviour (overridden per-drill/per-weapon at Play time by
        // Player's inline "weaponOverride" - see PatchPlayerWeapon).
        nlohmann::json playerConfig = {
            { "actions", { { "jump", "Jump" }, { "forward", "MoveForward" }, { "backward", "MoveBackward" },
                           { "left", "MoveLeft" }, { "right", "MoveRight" } } },
            { "audio", { { "jump", "jump" }, { "footsteps", "step" } } },
            { "weapon", { { "fireAction", "Fire" }, { "fireInterval", 0.15 }, { "semiAuto", true },
                          { "animation", "shoot" }, { "audio", "shoot" },
                          { "weaponObject", "Gun" }, { "muzzleObject", "Muzzle" },
                          { "hiddenObjects", nlohmann::json::array() } } },
        };
        if (!WriteTextFileAtomic(project.m_paths.Resolve("config/player.json"), playerConfig.dump(2), &error))
        {
            return { false, error };
        }

        project.AddDrill("Target Practice");

        if (!project.Save(&error))
        {
            return { false, error };
        }

        out = std::move(project);
        return { true, {} };
    }

    bool CourseProject::Save(std::string* error)
    {
        if (!m_paths.IsValid())
        {
            if (error) *error = "project has no folder";
            return false;
        }
        m_paths.EnsureLayout();

        for (auto& drill : drills)
        {
            if (drill.id.empty())
            {
                drill.id = MakeUniqueDrillId(Slugify(drill.name));
            }
            if (drill.sourceFile.empty())
            {
                drill.sourceFile = "drills/" + drill.id + ".json";
            }
            if (!WriteTextFileAtomic(m_paths.Resolve(drill.sourceFile), drill.ToJson().dump(2), error))
            {
                return false;
            }
        }

        return WriteTextFileAtomic(m_paths.ProjectFile(), ToProjectJson().dump(2), error);
    }

    bool CourseProject::SaveAs(const std::filesystem::path& newRoot, std::string* error)
    {
        std::error_code ec;
        std::filesystem::create_directories(newRoot, ec);
        if (ec)
        {
            if (error) *error = "cannot create " + newRoot.string();
            return false;
        }

        // Copy every existing project file (assets, scenes, ...) then rebind and
        // save so drill/project files reflect any unsaved edits.
        if (m_paths.IsValid() && std::filesystem::exists(m_paths.Root()))
        {
            std::filesystem::copy(m_paths.Root(), newRoot,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing, ec);
        }

        m_paths = ProjectPaths(newRoot);
        return Save(error);
    }

    bool CourseProject::WriteBackup(std::string* error)
    {
        if (!m_paths.IsValid())
        {
            if (error) *error = "project has no folder";
            return false;
        }
        nlohmann::json bundle = ToProjectJson();
        nlohmann::json drillsJson = nlohmann::json::array();
        for (const auto& d : drills)
        {
            drillsJson.push_back(d.ToJson());
        }
        bundle["drillsInline"] = drillsJson;
        bundle["backupOf"] = m_paths.Root().generic_string();

        const auto path = m_paths.BackupsDir() / (TimestampUtc() + ".json");
        return WriteTextFileAtomic(path, bundle.dump(2), error);
    }

    ProjectValidation CourseProject::Validate() const
    {
        using L = ProjectValidation::Level;
        ProjectValidation v;

        if (drills.empty())
        {
            v.issues.push_back({ L::Warning, "Project", "The course has no drills yet." });
        }
        if (drills.size() > 64)
        {
            v.issues.push_back({ L::Warning, "Project",
                "This course has " + std::to_string(drills.size()) +
                " drills; more than ~64 gets hard to navigate and slow to load." });
        }

        std::vector<std::string> seenIds;
        for (std::size_t i = 0; i < drills.size(); ++i)
        {
            const DrillDoc& d = drills[i];
            const std::string where = "Drill " + std::to_string(i + 1) + " '" + d.name + "'";

            if (d.id.empty())
            {
                v.issues.push_back({ L::Error, where, "Drill has no id." });
            }
            else if (std::find(seenIds.begin(), seenIds.end(), d.id) != seenIds.end())
            {
                v.issues.push_back({ L::Error, where, "Duplicate drill id '" + d.id + "'." });
            }
            seenIds.push_back(d.id);

            if (d.objectives.empty())
            {
                v.issues.push_back({ L::Error, where, "Drill has no objectives, so it can never be completed." });
            }
            if (d.targetTimeSeconds <= 0.0f)
            {
                v.issues.push_back({ L::Warning, where, "Par time is zero or negative; the time bonus will be disabled." });
            }

            int shootAmount = 0;
            for (const auto& o : d.objectives)
            {
                if (o.type != "shoot" && o.type != "knock" && o.type != "reach" && o.type != "finish")
                {
                    v.issues.push_back({ L::Error, where, "Unknown objective type '" + o.type + "'." });
                }
                if ((o.type == "shoot" || o.type == "knock") && o.amount <= 0)
                {
                    v.issues.push_back({ L::Error, where, "Objective '" + o.description + "' needs a positive amount." });
                }
                if (o.type == "shoot")
                {
                    shootAmount += o.amount;
                }
            }

            // Count shootable targets actually placed in the content.
            int targetCount = 0;
            if (d.content.is_array())
            {
                for (const auto& obj : d.content)
                {
                    if (!obj.contains("components") || !obj["components"].is_array())
                    {
                        continue;
                    }
                    for (const auto& c : obj["components"])
                    {
                        const std::string type = c.value("type", "");
                        if (type == "TargetComponent" || type == "MovingTargetComponent")
                        {
                            ++targetCount;
                        }
                    }
                }
            }
            if (shootAmount > targetCount)
            {
                v.issues.push_back({ L::Error, where,
                    "Objectives ask for " + std::to_string(shootAmount) +
                    " target hits but only " + std::to_string(targetCount) +
                    " shootable target(s) are placed." });
            }

            for (const auto& weaponId : d.allowedWeapons)
            {
                if (!FindWeapon(weaponId))
                {
                    v.issues.push_back({ L::Error, where,
                        "Allowed weapon '" + weaponId + "' is not defined in the project." });
                }
            }
        }

        auto conflicts = bindings.Conflicts();
        if (!conflicts.empty())
        {
            std::string joined;
            for (const auto& c : conflicts) { joined += (joined.empty() ? "" : ", ") + c; }
            v.issues.push_back({ L::Warning, "Key bindings", "These actions share a key: " + joined });
        }

        return v;
    }

    bool ProjectValidation::HasErrors() const
    {
        return Count(Level::Error) > 0;
    }

    int ProjectValidation::Count(Level level) const
    {
        int n = 0;
        for (const auto& issue : issues)
        {
            if (issue.level == level) ++n;
        }
        return n;
    }

    std::string CourseProject::MakeUniqueDrillId(const std::string& base) const
    {
        std::string candidate = base;
        int suffix = 2;
        auto taken = [this](const std::string& id)
        {
            return std::any_of(drills.begin(), drills.end(),
                [&](const DrillDoc& d) { return d.id == id; });
        };
        while (taken(candidate))
        {
            candidate = base + "-" + std::to_string(suffix++);
        }
        return candidate;
    }

    DrillDoc& CourseProject::AddDrill(const std::string& displayName)
    {
        DrillDoc d;
        d.name = displayName.empty() ? ("Drill " + std::to_string(drills.size() + 1)) : displayName;
        d.id = MakeUniqueDrillId(Slugify(d.name));
        d.sourceFile = "drills/" + d.id + ".json";
        d.objectives.push_back({ "finish", "Complete the drill", 1, "", "" });
        drills.push_back(std::move(d));
        return drills.back();
    }

    bool CourseProject::RemoveDrill(std::size_t index)
    {
        if (index >= drills.size())
        {
            return false;
        }
        // Delete the backing file too so a re-save does not resurrect it.
        if (m_paths.IsValid() && !drills[index].sourceFile.empty())
        {
            std::error_code ec;
            std::filesystem::remove(m_paths.Resolve(drills[index].sourceFile), ec);
        }
        drills.erase(drills.begin() + static_cast<long>(index));
        return true;
    }

    bool CourseProject::MoveDrill(std::size_t index, int delta)
    {
        if (index >= drills.size())
        {
            return false;
        }
        const long target = static_cast<long>(index) + delta;
        if (target < 0 || target >= static_cast<long>(drills.size()))
        {
            return false;
        }
        std::swap(drills[index], drills[static_cast<std::size_t>(target)]);
        return true;
    }

    DrillDoc* CourseProject::DuplicateDrill(std::size_t index)
    {
        if (index >= drills.size())
        {
            return nullptr;
        }
        DrillDoc copy = drills[index];
        copy.name = drills[index].name + " Copy";
        copy.id = MakeUniqueDrillId(Slugify(copy.name));
        copy.sourceFile = "drills/" + copy.id + ".json";
        drills.insert(drills.begin() + static_cast<long>(index) + 1, std::move(copy));
        return &drills[index + 1];
    }

    DrillDoc* CourseProject::FindDrill(const std::string& id)
    {
        for (auto& d : drills)
        {
            if (d.id == id) return &d;
        }
        return nullptr;
    }

    const WeaponDef* CourseProject::FindWeapon(const std::string& id) const
    {
        for (const auto& w : weapons)
        {
            if (w.id == id) return &w;
        }
        return nullptr;
    }
}
