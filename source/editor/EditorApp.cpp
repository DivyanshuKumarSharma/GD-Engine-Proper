#include "editor/EditorApp.h"

#include "editor/EditorCommands.h"
#include "editor/EditorLog.h"
#include "editor/PrimitiveFactory.h"
#include "project/WeaponCatalog.h"

#include "Bullet.h"
#include "JumpPlatform.h"
#include "Player.h"
#include "course/CourseDirector.h"
#include "hud/CrosshairComponent.h"
#include "hud/HudPanelComponent.h"
#include "targets/KnockTargetComponent.h"
#include "targets/MovingTargetComponent.h"
#include "targets/ReachZoneComponent.h"
#include "targets/TargetComponent.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace editor
{
    namespace
    {
        std::string g_iniPath; // must outlive ImGui context (io.IniFilename is a raw pointer)

        glm::vec3 JsonVec3(const nlohmann::json& j, const char* key, const glm::vec3& fb)
        {
            if (!j.contains(key) || !j[key].is_object()) return fb;
            const auto& v = j[key];
            return glm::vec3(v.value("x", fb.x), v.value("y", fb.y), v.value("z", fb.z));
        }

        // Local half-extents implied by a serialized MeshComponent, for picking.
        glm::vec3 MeshHalfExtents(const nlohmann::json& objectDoc)
        {
            if (!objectDoc.contains("components") || !objectDoc["components"].is_array())
            {
                return glm::vec3(0.35f);
            }
            for (const auto& c : objectDoc["components"])
            {
                if (c.value("type", "") != "MeshComponent" || !c.contains("mesh")) continue;
                const auto& m = c["mesh"];
                const std::string type = m.value("type", "box");
                if (type == "box")
                    return 0.5f * glm::vec3(m.value("x", 1.0f), m.value("y", 1.0f), m.value("z", 1.0f));
                if (type == "sphere")
                    return glm::vec3(m.value("r", 1.0f));
                if (type == "quad" || type == "square")
                    return glm::vec3(0.5f * m.value("width", 1.0f), 0.5f * m.value("height", 1.0f), 0.05f);
                if (type == "disc" || type == "circle" || type == "polygon" || type == "triangle" || type == "ngon")
                {
                    const float r = m.value("radius", 0.5f);
                    return glm::vec3(r, r, 0.05f);
                }
                if (type == "cylinder")
                {
                    const float r = m.value("radius", 0.5f);
                    return glm::vec3(r, 0.5f * m.value("height", 1.0f), r);
                }
            }
            return glm::vec3(0.35f);
        }

        bool RayAabb(const glm::vec3& o, const glm::vec3& d, const glm::vec3& lo, const glm::vec3& hi, float& tHit)
        {
            float tmin = 0.0f;
            float tmax = 1e30f;
            for (int i = 0; i < 3; ++i)
            {
                if (std::fabs(d[i]) < 1e-8f)
                {
                    if (o[i] < lo[i] || o[i] > hi[i]) return false;
                }
                else
                {
                    float t1 = (lo[i] - o[i]) / d[i];
                    float t2 = (hi[i] - o[i]) / d[i];
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = std::max(tmin, t1);
                    tmax = std::min(tmax, t2);
                    if (tmin > tmax) return false;
                }
            }
            tHit = tmin;
            return true;
        }

        // Finds the MainPlayer object's doc anywhere in a scene-schema tree.
        nlohmann::json* FindPlayerNode(nlohmann::json& node)
        {
            if (node.is_object())
            {
                if (node.value("type", "") == "Player" || node.value("name", "") == "MainPlayer")
                {
                    return &node;
                }
                for (auto& [key, value] : node.items())
                {
                    (void)key;
                    if (value.is_structured())
                    {
                        if (auto* found = FindPlayerNode(value)) return found;
                    }
                }
            }
            else if (node.is_array())
            {
                for (auto& item : node)
                {
                    if (auto* found = FindPlayerNode(item)) return found;
                }
            }
            return nullptr;
        }

        // Bakes authored movement settings into a scene-doc's Player.
        void PatchPlayerMovement(nlohmann::json& sceneDoc, const game::MovementSettings& mv)
        {
            nlohmann::json* player = FindPlayerNode(sceneDoc);
            if (!player || !player->contains("components") || !(*player)["components"].is_array())
            {
                return;
            }
            for (auto& c : (*player)["components"])
            {
                if (c.value("type", "") != "PlayerControllerComponent") continue;
                c["moveModel"] = "accel";
                c["moveSpeed"] = mv.moveSpeed;
                c["jumpHeight"] = mv.jumpHeight;
                c["groundAccel"] = mv.groundAccel;
                c["groundDecel"] = mv.groundDecel;
                c["friction"] = mv.friction;
                c["airControl"] = mv.airControl;
                c["lookSensitivity"] = mv.mouseSensitivity * 6.0f; // map to engine units
                return;
            }
        }

        // Attaches a visible weapon model to the Player and bakes a
        // "weaponOverride" so the drill/project's authored WeaponDef (fire mode,
        // rate, zoom, sounds) actually drives play - see Player::LoadProperties.
        void PatchPlayerWeapon(nlohmann::json& sceneDoc, const game::WeaponDef& weapon)
        {
            nlohmann::json* player = FindPlayerNode(sceneDoc);
            if (!player)
            {
                return;
            }

            std::string audioClipName;
            if (!weapon.fireSound.empty() && player->contains("components") && (*player)["components"].is_array())
            {
                for (auto& c : (*player)["components"])
                {
                    if (c.value("type", "") != "AudioComponent") continue;
                    if (!c.contains("audio") || !c["audio"].is_array()) c["audio"] = nlohmann::json::array();
                    audioClipName = "weapon_" + weapon.id;
                    bool exists = false;
                    for (const auto& clip : c["audio"])
                    {
                        if (clip.value("name", "") == audioClipName) { exists = true; break; }
                    }
                    if (!exists)
                    {
                        c["audio"].push_back({ { "name", audioClipName }, { "path", weapon.fireSound } });
                    }
                    break;
                }
            }

            nlohmann::json weaponOverride = game::WeaponCatalog::PlayerOverrideDoc(weapon);
            if (!audioClipName.empty())
            {
                weaponOverride["audio"] = audioClipName;
            }
            (*player)["weaponOverride"] = std::move(weaponOverride);

            // Replace any previously-attached weapon model so switching weapons
            // between play sessions doesn't leave a stale one behind.
            if (!player->contains("children") || !(*player)["children"].is_array())
            {
                (*player)["children"] = nlohmann::json::array();
            }
            auto& kids = (*player)["children"];
            kids.erase(std::remove_if(kids.begin(), kids.end(),
                [](const nlohmann::json& c) { return c.value("name", "") == "Gun"; }), kids.end());
            kids.push_back(game::WeaponCatalog::EquipModel(weapon));
        }

        // Drops the editor's live drill-content layer from a scene doc. During
        // play the CourseDirector/DrillRunner is the sole spawner of drill
        // content (from the DrillDefinition, not a pre-baked copy) - keeping both
        // would duplicate every target.
        void StripDrillRoot(nlohmann::json& sceneDoc)
        {
            if (!sceneDoc.contains("objects") || !sceneDoc["objects"].is_array())
            {
                return;
            }
            auto& objects = sceneDoc["objects"];
            objects.erase(std::remove_if(objects.begin(), objects.end(),
                [](const nlohmann::json& o) { return o.value("name", "") == "DrillRoot"; }), objects.end());
        }

#if defined(_WIN32)
        bool PickFolderNative(std::string& out)
        {
            wchar_t path[MAX_PATH] = { 0 };
            BROWSEINFOW bi = { 0 };
            bi.lpszTitle = L"Select a folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (!pidl) return false;
            const bool ok = SHGetPathFromIDListW(pidl, path);
            CoTaskMemFree(pidl);
            if (!ok) return false;
            char utf8[MAX_PATH * 3] = { 0 };
            WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
            out = utf8;
            return true;
        }
#else
        bool PickFolderNative(std::string&) { return false; }
#endif
    }

    EditorApp::EditorApp() = default;
    EditorApp::~EditorApp() = default;

    void EditorApp::RegisterTypes()
    {
        Player::Register();
        Bullet::Register();
        JumpPlatform::Register();
        game::TargetComponent::Register();
        game::MovingTargetComponent::Register();
        game::KnockTargetComponent::Register();
        game::ReachZoneComponent::Register();
        game::CrosshairComponent::Register();
        game::HudPanelComponent::Register();
    }

    bool EditorApp::Init()
    {
        EditorLog::CaptureStandardError();
        EditorLog::Info("editor", "CourseMaker starting up.");

        auto& engine = eng::Engine::GetInstance();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        g_iniPath = (engine.GetFileSystem().GetExecutableFolder() / "coursemaker_layout.ini").string();
        io.IniFilename = g_iniPath.c_str();

        ApplyTheme();

        ImGui_ImplGlfw_InitForOpenGL(engine.GetWindow(), true);
        ImGui_ImplOpenGL3_Init("#version 330");

        m_viewportFB.Resize(1280, 720);
        engine.SetSceneRenderTarget(&m_viewportFB);
        engine.GetInputManager().SetSuppressed(true);
        engine.GetPhysicsManager().SetPaused(true);

        m_camera.Reset();

        // Empty holding scene until a project is opened.
        m_scene = std::make_shared<eng::Scene>();
        engine.SetScene(m_scene);

        std::filesystem::path startup = m_startupProject;
        if (startup.empty())
        {
            // The bundled example project ships inside the assets folder.
            auto bundled = engine.GetFileSystem().GetAssetsFolder();
            if (std::filesystem::exists(bundled / "project.json"))
            {
                startup = bundled;
            }
        }
        if (!startup.empty())
        {
            OpenProject(startup);
        }
        else
        {
            m_statusLine = "No project open - use File > New Project or File > Open Project.";
        }

        return true;
    }

    // ----------------------------------------------------------------- project
    bool EditorApp::OpenProject(const std::filesystem::path& root)
    {
        if (m_play.IsPlaying())
        {
            ExitPlay();
        }

        CourseProject loaded;
        auto result = CourseProject::Load(root, loaded);
        if (!result.ok)
        {
            EditorLog::Error("project", "Open failed: " + result.error);
            m_statusLine = "Open failed: " + result.error;
            return false;
        }

        m_project = std::move(loaded);
        m_hasProject = true;
        m_selection = 0;
        m_activeDrill = -1;
        m_commands.Clear();

        auto& fs = eng::Engine::GetInstance().GetFileSystem();
        fs.SetAssetsRootOverride(m_project.Paths().Root());
        m_inputMapLoaded = LoadProjectInputMap();

        LoadEnvironmentScene();
        RefreshAssets();
        m_camera.SetPose(glm::vec3(10.0f, 9.0f, 20.0f), -22.0f, -20.0f);
        if (!m_project.drills.empty())
        {
            SelectDrill(0);
        }

        auto validation = m_project.Validate();
        for (const auto& issue : validation.issues)
        {
            const auto level = issue.level == ProjectValidation::Level::Error ? LogLevel::Error
                : issue.level == ProjectValidation::Level::Warning ? LogLevel::Warning : LogLevel::Info;
            EditorLog::Add(level, "validation", issue.where + ": " + issue.message);
        }

        EditorLog::Success("project", "Opened '" + m_project.name + "' (" +
            std::to_string(m_project.drills.size()) + " drill(s)).");
        m_statusLine = "Opened " + m_project.Paths().Root().string();
        return true;
    }

    bool EditorApp::LoadProjectInputMap()
    {
        auto& input = eng::Engine::GetInstance().GetInputManager();
        const auto path = m_project.Paths().Resolve("config/input.json");
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            input.ClearActionMap();
            EditorLog::Warn("input", "config/input.json not found - movement, firing, pause and "
                "confirm will not respond during Play until one exists.");
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string error;
        if (!input.LoadActionMap(ss.str(), &error))
        {
            EditorLog::Error("input", "config/input.json is invalid: " + error);
            return false;
        }
        return true;
    }

    void EditorApp::LoadEnvironmentScene()
    {
        auto& engine = eng::Engine::GetInstance();
        const auto scenePath = m_project.Paths().Resolve(m_project.environmentScene);

        std::shared_ptr<eng::Scene> scene;
        std::ifstream file(scenePath, std::ios::binary);
        if (file.is_open())
        {
            std::stringstream ss;
            ss << file.rdbuf();
            try
            {
                scene = eng::Scene::CreateFromDocument(nlohmann::json::parse(ss.str()));
            }
            catch (const std::exception& e)
            {
                EditorLog::Error("scene", std::string("environment scene is malformed: ") + e.what());
            }
        }
        else
        {
            EditorLog::Warn("scene", "environment scene not found: " + m_project.environmentScene);
        }

        if (!scene)
        {
            scene = std::make_shared<eng::Scene>();
        }
        m_scene = scene;
        engine.SetScene(m_scene);
    }

    void EditorApp::SeedDefaultAssets(const std::filesystem::path& projectRoot)
    {
        static const char* kManifest[] = {
            "materials/checker.mat",
            "materials/suzanne.mat",
            "textures/checker.png",
            "shaders/vertex.glsl",
            "shaders/fragment.glsl",
            "audio/jump.wav",
            "audio/shoot.wav",
            "audio/step.wav",
            "fonts/arial.ttf",
            "models/suzanne/Suzanne_BaseColor.png",
        };

        const auto source = eng::Engine::GetInstance().GetFileSystem().GetDefaultAssetsFolder();
        std::error_code ec;
        for (const char* relative : kManifest)
        {
            const auto from = source / relative;
            const auto to = projectRoot / relative;
            if (!std::filesystem::exists(from, ec))
            {
                continue; // this CourseMaker install doesn't ship that default; skip it
            }
            std::filesystem::create_directories(to.parent_path(), ec);
            std::filesystem::copy_file(from, to, std::filesystem::copy_options::skip_existing, ec);
            if (ec)
            {
                EditorLog::Warn("project", "Could not seed default asset '" + std::string(relative) + "': " + ec.message());
                ec.clear();
            }
        }
    }

    void EditorApp::RefreshAssets()
    {
        if (m_hasProject)
        {
            m_assets.Rescan(m_project.Paths().Root());
        }
    }

    // ------------------------------------------------------------- drill layers
    eng::GameObject* EditorApp::DrillRoot() const
    {
        return m_scene ? m_scene->FindObjectByName("DrillRoot") : nullptr;
    }

    void EditorApp::FlushActiveDrill()
    {
        if (m_activeDrill < 0 || m_activeDrill >= static_cast<int>(m_project.drills.size()))
        {
            return;
        }
        eng::GameObject* root = DrillRoot();
        nlohmann::json content = nlohmann::json::array();
        if (root)
        {
            for (const auto& child : root->GetChildren())
            {
                nlohmann::json objDoc = child->Serialize();
                objDoc.erase("id");
                content.push_back(std::move(objDoc));
            }
        }
        m_project.drills[m_activeDrill].content = std::move(content);
    }

    void EditorApp::LoadActiveDrillLayer()
    {
        if (!m_scene)
        {
            return;
        }
        if (eng::GameObject* stale = DrillRoot())
        {
            m_scene->DestroyImmediate(stale);
        }
        if (m_activeDrill < 0 || m_activeDrill >= static_cast<int>(m_project.drills.size()))
        {
            return;
        }

        eng::GameObject* root = m_scene->CreateObject("DrillRoot");
        const auto& drill = m_project.drills[m_activeDrill];
        if (drill.content.is_array())
        {
            for (const auto& objDoc : drill.content)
            {
                m_scene->LoadObjectFromJson(objDoc, root);
            }
        }
        EditorLog::Info("drill", "Loaded drill '" + drill.name + "' (" +
            std::to_string(drill.content.is_array() ? drill.content.size() : 0) + " object(s)).");
    }

    void EditorApp::SelectDrill(int index)
    {
        if (m_play.IsPlaying())
        {
            return;
        }
        FlushActiveDrill();
        m_activeDrill = index;
        m_selection = 0;
        LoadActiveDrillLayer();
    }

    bool EditorApp::SaveProject()
    {
        if (!m_hasProject)
        {
            return false;
        }
        if (m_play.IsPlaying())
        {
            EditorLog::Warn("project", "Stop play mode before saving.");
            return false;
        }

        // Fold the live drill layer back into its DrillDoc.
        FlushActiveDrill();

        // Persist the working scene as the environment shell, minus the drill
        // layer (that belongs to the drill files, not the environment).
        nlohmann::json sceneDoc = m_scene->Serialize();
        StripDrillRoot(sceneDoc);
        const auto scenePath = m_project.Paths().Resolve(m_project.environmentScene);
        std::error_code ec;
        std::filesystem::create_directories(scenePath.parent_path(), ec);
        {
            std::ofstream out(scenePath, std::ios::binary | std::ios::trunc);
            out << sceneDoc.dump(2);
        }

        std::string error;
        if (!m_project.Save(&error))
        {
            EditorLog::Error("project", "Save failed: " + error);
            m_statusLine = "Save failed: " + error;
            return false;
        }
        m_project.WriteBackup(nullptr);
        m_commands.MarkSaved();
        EditorLog::Success("project", "Saved '" + m_project.name + "'.");
        m_statusLine = "Saved " + m_project.Paths().Root().string();
        return true;
    }

    bool EditorApp::SaveProjectAs()
    {
        std::string folder;
        if (!PickFolderNative(folder) || folder.empty())
        {
            return false;
        }
        std::string error;
        if (!m_project.SaveAs(std::filesystem::path(folder) / m_project.name, &error))
        {
            EditorLog::Error("project", "Save As failed: " + error);
            return false;
        }
        eng::Engine::GetInstance().GetFileSystem().SetAssetsRootOverride(m_project.Paths().Root());
        RefreshAssets();
        m_commands.MarkSaved();
        EditorLog::Success("project", "Saved a copy to " + m_project.Paths().Root().string());
        return true;
    }

    void EditorApp::NewProjectFlow()
    {
        m_openNewProjectModal = true;
        std::snprintf(m_newProjectName, sizeof(m_newProjectName), "%s", "My Course");
        m_newProjectPath[0] = 0;
    }

    void EditorApp::OpenProjectFlow()
    {
        std::string folder;
        if (PickFolderNative(folder) && !folder.empty())
        {
            OpenProject(folder);
        }
    }

    // ------------------------------------------------------------------ editing
    eng::GameObject* EditorApp::Selected() const
    {
        return m_scene ? m_scene->FindObjectById(m_selection) : nullptr;
    }

    void EditorApp::Select(uint64_t id)
    {
        m_selection = id;
    }

    void EditorApp::AddObjectFromDoc(const nlohmann::json& doc, const std::string& label, uint64_t parentId)
    {
        if (!m_scene || m_play.IsPlaying())
        {
            return;
        }
        auto command = std::make_unique<CreateObjectCommand>(m_scene.get(), parentId, doc, "Add " + label);
        auto* raw = command.get();
        m_commands.Execute(std::move(command));
        Select(raw->CreatedId());
        EditorLog::Info("edit", "Added " + label + ".");
    }

    void EditorApp::DeleteSelected()
    {
        if (!m_scene || m_selection == 0 || m_play.IsPlaying())
        {
            return;
        }
        auto command = std::make_unique<DeleteObjectCommand>(m_scene.get(), m_selection);
        if (!command->Valid())
        {
            return;
        }
        m_commands.Execute(std::move(command));
        Select(0);
    }

    void EditorApp::DuplicateSelected()
    {
        eng::GameObject* object = Selected();
        if (!object || m_play.IsPlaying())
        {
            return;
        }
        nlohmann::json doc = object->Serialize();
        doc.erase("id");
        if (doc.contains("name"))
        {
            doc["name"] = doc["name"].get<std::string>() + " Copy";
        }
        glm::vec3 p = object->GetPosition() + glm::vec3(1.0f, 0.0f, 1.0f);
        doc["position"] = { { "x", p.x }, { "y", p.y }, { "z", p.z } };

        const uint64_t parentId = object->GetParent() ? object->GetParent()->GetId() : 0;
        auto command = std::make_unique<CreateObjectCommand>(m_scene.get(), parentId, doc, "Duplicate");
        auto* raw = command.get();
        m_commands.Execute(std::move(command));
        Select(raw->CreatedId());
    }

    void EditorApp::FocusSelected()
    {
        if (eng::GameObject* object = Selected())
        {
            m_camera.Focus(object->GetWorldPosition(), 6.0f);
        }
    }

    void EditorApp::PushTransformEdit(eng::GameObject* object,
        const glm::vec3& oldP, const glm::quat& oldR, const glm::vec3& oldS)
    {
        if (!object)
        {
            return;
        }
        m_commands.Push(std::make_unique<TransformCommand>(m_scene.get(), object->GetId(),
            oldP, oldR, oldS, object->GetPosition(), object->GetRotation(), object->GetScale()));
    }

    // --------------------------------------------------------------------- play
    const WeaponDef* EditorApp::ResolveActiveWeapon() const
    {
        if (m_activeDrill >= 0 && m_activeDrill < static_cast<int>(m_project.drills.size()))
        {
            const auto& allowed = m_project.drills[m_activeDrill].allowedWeapons;
            if (!allowed.empty())
            {
                if (const auto* weapon = m_project.FindWeapon(allowed.front()))
                {
                    return weapon;
                }
            }
        }
        return m_project.weapons.empty() ? nullptr : &m_project.weapons.front();
    }

    void EditorApp::EnterPlay(bool wholeCourse)
    {
        if (!m_scene || m_play.IsPlaying() || !m_hasProject)
        {
            return;
        }
        if (!wholeCourse && (m_activeDrill < 0 || m_activeDrill >= static_cast<int>(m_project.drills.size())))
        {
            EditorLog::Warn("play", "No active drill to play - select one in the Course panel.");
            return;
        }
        if (wholeCourse && m_project.drills.empty())
        {
            EditorLog::Warn("play", "The course has no drills yet.");
            return;
        }

        m_placement.Abort(m_scene.get());
        FlushActiveDrill();
        if (!m_inputMapLoaded)
        {
            m_inputMapLoaded = LoadProjectInputMap();
        }

        game::MovementSettings mv = m_project.movementDefaults;
        if (m_activeDrill >= 0 && m_activeDrill < static_cast<int>(m_project.drills.size()))
        {
            mv = m_project.drills[m_activeDrill].ResolvedMovement(m_project.movementDefaults);
        }
        const WeaponDef* weapon = ResolveActiveWeapon();
        WeaponDef weaponCopy;
        if (weapon) weaponCopy = *weapon;
        const bool hasWeapon = weapon != nullptr;

        auto playScene = m_play.EnterPlay(m_scene.get(),
            [mv, hasWeapon, weaponCopy](nlohmann::json& doc)
            {
                StripDrillRoot(doc);
                PatchPlayerMovement(doc, mv);
                if (hasWeapon)
                {
                    PatchPlayerWeapon(doc, weaponCopy);
                }
            });
        if (!playScene)
        {
            return;
        }
        m_scene = playScene;
        m_lastPlayWasWholeCourse = wholeCourse;
        auto& engine = eng::Engine::GetInstance();
        engine.SetScene(m_scene);
        engine.SetCameraOverride(nullptr);       // use the scene's own (player) camera
        engine.GetInputManager().SetSuppressed(false);

        BeginDirectorRun(wholeCourse);
        m_statusLine = "Playing - Esc to pause (and reach Stop), Enter to confirm.";
    }

    void EditorApp::BeginDirectorRun(bool wholeCourse)
    {
        eng::GameObject* playerObject = m_scene->FindObjectByName("MainPlayer");
        Player* player = (playerObject && std::string(playerObject->GetTypeName()) == "Player")
            ? static_cast<Player*>(playerObject) : nullptr;
        if (!player)
        {
            EditorLog::Error("play", "Scene has no 'MainPlayer' Player object - cannot start a run.");
        }

        std::vector<game::DrillDefinition> drills;
        if (wholeCourse)
        {
            drills.reserve(m_project.drills.size());
            for (const auto& drill : m_project.drills) drills.push_back(drill.ToRuntime());
        }
        else
        {
            drills.push_back(m_project.drills[m_activeDrill].ToRuntime());
        }

        m_director = std::make_unique<game::CourseDirector>();
        m_director->SetFastTransitions(m_demo); // instant countdown for the automated workflow test
        m_director->onQuitRequested = [this]() { ExitPlay(); };
        m_director->Init(m_scene.get(), player, nullptr, std::move(drills));
        if (wholeCourse)
        {
            m_director->StartRun();
        }
        else
        {
            m_director->BeginDrill(0);
        }
    }

    void EditorApp::RestartPlay()
    {
        if (!m_play.IsPlaying())
        {
            return;
        }
        auto scene = m_play.Restart();
        if (!scene)
        {
            return;
        }
        m_scene = scene;
        eng::Engine::GetInstance().SetScene(m_scene);
        BeginDirectorRun(m_lastPlayWasWholeCourse);
    }

    void EditorApp::ExitPlay()
    {
        if (!m_play.IsPlaying())
        {
            return;
        }
        m_director.reset();
        auto editScene = m_play.ExitPlay();
        m_scene = editScene ? editScene : std::make_shared<eng::Scene>();
        auto& engine = eng::Engine::GetInstance();
        engine.SetScene(m_scene);
        engine.GetInputManager().SetSuppressed(true);
        engine.SetCursorEnabled(true);
        m_statusLine = "Returned to editor.";
    }

    void EditorApp::TogglePauseDirected()
    {
        if (!m_director)
        {
            m_play.TogglePause();
            return;
        }
        m_director->OnPausePressed();
        m_play.SetPaused(m_director->State() == game::GameState::Paused);
    }

    // ------------------------------------------------------------------- update
    void EditorApp::Update(float deltaTime)
    {
        m_clock += deltaTime;
        EditorLog::Tick(deltaTime);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        auto& engine = eng::Engine::GetInstance();
        ImGuiIO& io = ImGui::GetIO();

        // --- input routing -------------------------------------------------
        if (m_play.IsPlaying())
        {
            const bool gameHasFocus = m_viewportFocused || glfwGetInputMode(engine.GetWindow(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
            engine.GetInputManager().SetSuppressed(!gameHasFocus || io.WantTextInput);

            // Raw key checks (not the suppressible action map) so pause/confirm
            // always work even if a project has no config/input.json yet.
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                TogglePauseDirected();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) && m_director)
            {
                m_director->OnConfirmPressed();
            }
        }
        else
        {
            engine.GetInputManager().SetSuppressed(true);
        }

        HandleShortcuts();

        // --- UI ----------------------------------------------------------
        DrawDockSpaceAndMenu();
        DrawToolbar();
        DrawViewport();
        DrawHierarchy();
        DrawInspector();
        DrawAssetBrowser();
        DrawCoursePanel();
        DrawDiagnostics();
        DrawModals();
        if (m_showImGuiDemo)
        {
            ImGui::ShowDemoWindow(&m_showImGuiDemo);
        }

        // Scripted workflow edits run here - BEFORE the render feed below - so a
        // structural change never leaves a dangling command in the render queue.
        if (m_demo)
        {
            RunDemo();
        }

        // --- camera + scene render feed --------------------------------
        const float aspect = m_viewportSize.y > 0.0f ? m_viewportSize.x / m_viewportSize.y : 1.0f;
        m_viewportFB.Resize(static_cast<int>(m_viewportSize.x), static_cast<int>(m_viewportSize.y));

        if (m_play.IsPlaying())
        {
            // Director first (may spawn/despawn drill content, teleport the
            // player, change state) so the scene tick right after never runs
            // mid-transition - mirrors the standalone game's Update order.
            if (m_director)
            {
                const float dt = std::min(deltaTime, 0.1f);
                m_director->Update(dt);
            }
            m_play.Tick(deltaTime, m_scene.get());
        }
        else
        {
            const bool interacting = m_viewportHovered && !ImGuizmo::IsUsing();
            m_camera.Update(deltaTime, interacting);
            m_activeCameraData = m_camera.BuildCameraData(aspect);
            engine.SetCameraOverride(&m_activeCameraData);
            if (m_scene)
            {
                m_scene->RenderEditorPreview();
            }
        }

    }

    namespace
    {
        int CountSceneObjects(const eng::GameObject* obj)
        {
            int n = 1;
            for (const auto& child : obj->GetChildren())
            {
                n += CountSceneObjects(child.get());
            }
            return n;
        }
        int CountSceneObjects(const eng::Scene* scene)
        {
            int n = 0;
            for (const auto& root : scene->GetRootObjects())
            {
                n += CountSceneObjects(root.get());
            }
            return n;
        }
    }

    void EditorApp::RunDemo()
    {
        if (m_demoWait > 0)
        {
            --m_demoWait;
            return;
        }

        auto expect = [&](bool cond, const char* what)
        {
            if (!cond)
            {
                ++m_demoFails;
                EditorLog::Error("demo", std::string("CHECK FAILED: ") + what);
                std::printf("[demo] FAIL  %s\n", what);
            }
            else
            {
                EditorLog::Success("demo", std::string("ok: ") + what);
                std::printf("[demo] ok    %s\n", what);
            }
            std::fflush(stdout);
        };
        auto shot = [&](const char* tag)
        {
            if (!m_shotsDir.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(m_shotsDir, ec);
                eng::Engine::GetInstance().GetGraphicsAPI().SaveScreenshot(
                    (std::filesystem::path(m_shotsDir) / (std::string("demo_") + tag + ".png")).string());
            }
        };

        switch (m_demoStep++)
        {
        case 0:
        {
            expect(m_hasProject, "project opened on startup");
            expect(m_project.drills.size() == 3, "example project has 3 drills");
            // Work on a throw-away copy so the demo never mutates the shipped
            // example project.
            std::error_code ec;
            const auto sandbox = std::filesystem::temp_directory_path() / "coursemaker_demo_sandbox";
            std::filesystem::remove_all(sandbox, ec);
            std::string err;
            expect(m_project.SaveAs(sandbox, &err), "SaveAs to a sandbox folder");
            eng::Engine::GetInstance().GetFileSystem().SetAssetsRootOverride(m_project.Paths().Root());
            m_demoWait = 3;
            break;
        }
        case 1:
        {
            const int before = CountSceneObjects(m_scene.get());
            AddObjectFromDoc(PrimitiveFactory::MakeTarget("circle", glm::vec3(0, 2, 6)), "Circle Target");
            AddObjectFromDoc(PrimitiveFactory::MakeTarget("square", glm::vec3(2.5f, 2, 6)), "Square Target");
            AddObjectFromDoc(PrimitiveFactory::MakeTarget("triangle", glm::vec3(-2.5f, 2, 6)), "Triangle Target");
            expect(CountSceneObjects(m_scene.get()) == before + 3, "three targets added to the scene");
            expect(m_selection != 0, "last placed target is selected");
            m_demoWait = 3;
            break;
        }
        case 2:
            shot("targets");
            m_demoWait = 1;
            break;
        case 3:
        {
            const int before = CountSceneObjects(m_scene.get());
            m_commands.Undo();
            expect(CountSceneObjects(m_scene.get()) == before - 1, "undo removes the triangle target");
            m_commands.Redo();
            expect(CountSceneObjects(m_scene.get()) == before, "redo restores it");
            m_demoWait = 2;
            break;
        }
        case 4:
        {
            eng::GameObject* obj = Selected();
            expect(obj != nullptr, "an object is selected for the move test");
            if (obj)
            {
                const glm::vec3 start = obj->GetPosition();
                const glm::quat r0 = obj->GetRotation();
                const glm::vec3 s0 = obj->GetScale();
                obj->SetPosition(start + glm::vec3(0, 3, 0));
                PushTransformEdit(obj, start, r0, s0);
                expect(glm::length(Selected()->GetPosition() - (start + glm::vec3(0, 3, 0))) < 0.01f, "move applied");
                m_commands.Undo();
                expect(glm::length(Selected()->GetPosition() - start) < 0.01f, "undo restores position");
            }
            m_demoWait = 2;
            break;
        }
        case 5:
            expect(SaveProject(), "SaveProject() writes project + drills");
            expect(!m_commands.IsDirty(), "command stack is clean after save");
            m_demoWait = 2;
            break;
        case 6:
        {
            m_demoSnapshotCheck = m_scene->Serialize();
            EnterPlay(false); // playtest the active drill only
            expect(m_play.IsPlaying(), "entered play mode");
            expect(m_director != nullptr, "CourseDirector created for the playtest");
            m_demoPollFrames = 0;
            m_demoWait = 1;
            break;
        }
        case 7:
        {
            // Poll for Playing instead of guessing a frame count: correct at any
            // host frame rate, capped so a real failure still terminates.
            const bool ready = m_director && m_director->State() == game::GameState::Playing;
            if (!ready && ++m_demoPollFrames < 600)
            {
                --m_demoStep; // stay on this step next tick
            }
            else
            {
                expect(ready, "director reached Playing after intro + countdown");
            }
            m_demoWait = 1;
            break;
        }
        case 8:
        {
            eng::GameObject* drillRoot = m_scene->FindObjectByName("DrillRoot");
            const std::size_t expectedTargets = (m_activeDrill >= 0 &&
                m_activeDrill < static_cast<int>(m_project.drills.size()))
                ? m_project.drills[m_activeDrill].content.size() : 0;
            expect(drillRoot != nullptr, "DrillRunner spawned the drill's DrillRoot");
            expect(drillRoot && drillRoot->GetChildren().size() == expectedTargets,
                "drill content spawned exactly once (no duplicate from the editor's layer)");

            eng::GameObject* playerObject = m_scene->FindObjectByName("MainPlayer");
            eng::GameObject* gun = playerObject ? playerObject->FindChildByName("Gun") : nullptr;
            expect(gun != nullptr, "a weapon model is equipped on the player");

            eng::GameObject* cube = m_scene->FindObjectByName("D1_Cube_C");
            if (playerObject && std::string(playerObject->GetTypeName()) == "Player" && cube)
            {
                auto* player = static_cast<Player*>(playerObject);
                const glm::vec3 eye = player->GetWorldPosition();
                const glm::vec3 toTarget = cube->GetWorldPosition() - eye;
                if (glm::dot(toTarget, toTarget) > 1e-4f)
                {
                    player->InjectShot(eye, glm::normalize(toTarget));
                }
            }
            m_demoWait = 4; // let ShootingController resolve the injected shot
            break;
        }
        case 9:
            shot("play");
            expect(m_director && m_director->Stats().hits > 0,
                "hitscan shot fired from the spawned player registered a hit on a real drill target");
            expect(m_director && m_director->Stats().shotsFired > 0, "shots-fired stat updated");
            m_demoPollFrames = 0;
            m_demoWait = 1;
            break;
        case 10:
        {
            // Skip straight to "all objectives met" (same debug hook the
            // standalone game's bot uses) rather than reproducing every shot -
            // what's under test here is the state machine, not marksmanship.
            if (m_demoPollFrames == 0 && m_director)
            {
                m_director->DebugCompleteActiveDrill();
            }
            const bool ready = m_director && m_director->State() == game::GameState::DrillComplete;
            if (!ready && ++m_demoPollFrames < 600)
            {
                --m_demoStep;
            }
            else
            {
                expect(ready, "completing all objectives reached DrillComplete");
                m_demoPollFrames = 0;
            }
            m_demoWait = 1;
            break;
        }
        case 11:
        {
            // A single-drill playtest has nowhere to go after DrillComplete but
            // RunComplete once its dwell elapses - the results screen.
            const bool ready = m_director && m_director->State() == game::GameState::RunComplete;
            if (!ready && ++m_demoPollFrames < 600)
            {
                --m_demoStep;
            }
            else
            {
                expect(ready, "single-drill run reached RunComplete (results) after the drill-complete dwell");
                if (m_director)
                {
                    expect(!m_director->DrillResults().empty(), "a DrillResult was recorded");
                    expect(m_director->FinalScore().total >= 0, "final score computed");
                }
            }
            m_demoWait = 2;
            break;
        }
        case 12:
            shot("results");
            m_demoWait = 2;
            break;
        case 13:
        {
            ExitPlay();
            expect(!m_play.IsPlaying(), "returned to editor");
            expect(m_director == nullptr, "CourseDirector torn down on Stop");
            const auto restored = m_scene->Serialize();
            expect(restored.value("objects", nlohmann::json::array()).size() ==
                   m_demoSnapshotCheck.value("objects", nlohmann::json::array()).size(),
                   "authored scene object count restored after Stop");
            m_demoWait = 3;
            break;
        }
        case 14:
            shot("back");
            std::printf("\n[demo] %s (%d failed checks)\n",
                m_demoFails == 0 ? "DEMO WORKFLOW PASSED" : "DEMO WORKFLOW FAILED", m_demoFails);
            std::fflush(stdout);
            SetNeedsToBeClosed(true);
            break;
        default:
            SetNeedsToBeClosed(true);
            break;
        }
    }

    void EditorApp::OnPostRender()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (m_headlessFrames > 0)
        {
            ++m_frame;
            auto& gfx = eng::Engine::GetInstance().GetGraphicsAPI();
            if (!m_shotsDir.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(m_shotsDir, ec);
                if (m_frame == 8 || m_frame == m_headlessFrames - 2)
                {
                    const std::string path = (std::filesystem::path(m_shotsDir) /
                        ("editor_frame_" + std::to_string(m_frame) + ".png")).string();
                    gfx.SaveScreenshot(path);
                }
            }
            if (m_frame >= m_headlessFrames)
            {
                SetNeedsToBeClosed(true);
            }
        }
    }

    void EditorApp::Destroy()
    {
        if (m_play.IsPlaying())
        {
            ExitPlay();
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        EditorLog::ReleaseStandardError();
    }

    // ---------------------------------------------------------------- shortcuts
    void EditorApp::HandleShortcuts()
    {
        if (m_play.IsPlaying())
        {
            return;
        }
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput)
        {
            return;
        }
        const bool ctrl = io.KeyCtrl;

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) { m_commands.Undo(); }
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))) { m_commands.Redo(); }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) { SaveProject(); }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) { DuplicateSelected(); }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) { DeleteSelected(); }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) { FocusSelected(); }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { m_gizmoOperation = ImGuizmo::TRANSLATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { m_gizmoOperation = ImGuizmo::ROTATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_gizmoOperation = ImGuizmo::SCALE; }
    }

    // -------------------------------------------------------------------- theme
    void EditorApp::ApplyTheme()
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 4.0f;
        s.FrameRounding = 4.0f;
        s.GrabRounding = 4.0f;
        s.TabRounding = 4.0f;
        s.ScrollbarRounding = 4.0f;
        s.WindowPadding = ImVec2(10, 10);
        s.FramePadding = ImVec2(8, 5);
        s.ItemSpacing = ImVec2(8, 7);
        s.WindowBorderSize = 1.0f;
        s.WindowMenuButtonPosition = ImGuiDir_None;

        ImVec4* c = s.Colors;
        const ImVec4 bg = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
        const ImVec4 panel = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        const ImVec4 accent = ImVec4(0.16f, 0.72f, 0.86f, 1.00f);   // cyan
        const ImVec4 accentDim = ImVec4(0.16f, 0.72f, 0.86f, 0.35f);
        c[ImGuiCol_WindowBg] = bg;
        c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
        c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.14f, 0.98f);
        c[ImGuiCol_Border] = ImVec4(0.24f, 0.26f, 0.30f, 0.60f);
        c[ImGuiCol_FrameBg] = panel;
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.27f, 0.33f, 1.0f);
        c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);
        c[ImGuiCol_Header] = accentDim;
        c[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.72f, 0.86f, 0.55f);
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.29f, 0.35f, 1.0f);
        c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.85f, 0.95f, 1.0f);
        c[ImGuiCol_Tab] = ImVec4(0.13f, 0.14f, 0.17f, 1.0f);
        c[ImGuiCol_TabHovered] = accentDim;
        c[ImGuiCol_TabSelected] = ImVec4(0.18f, 0.30f, 0.34f, 1.0f);
        c[ImGuiCol_TabDimmed] = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
        c[ImGuiCol_DockingPreview] = accentDim;
        c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.94f, 1.0f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.56f, 1.0f);
        c[ImGuiCol_SeparatorHovered] = accent;
    }

    // =====================================================================
    //  Panels
    // =====================================================================
    namespace
    {
        const ImVec4 kCyan(0.20f, 0.80f, 0.92f, 1.0f);
        const ImVec4 kViolet(0.62f, 0.47f, 0.95f, 1.0f);
        const ImVec4 kGreen(0.36f, 0.83f, 0.44f, 1.0f);
        const ImVec4 kWarm(0.96f, 0.68f, 0.28f, 1.0f);
        const ImVec4 kRed(0.93f, 0.35f, 0.35f, 1.0f);

        ImVec4 LevelColour(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Success: return kGreen;
            case LogLevel::Warning: return kWarm;
            case LogLevel::Error: return kRed;
            default: return ImVec4(0.75f, 0.78f, 0.82f, 1.0f);
            }
        }
    }

    void EditorApp::DrawDockSpaceAndMenu()
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImGuiID dockId = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_None);

        static bool builtOnce = false;
        const bool needBuild = m_rebuildLayout || (!builtOnce && !std::filesystem::exists(g_iniPath));
        if (needBuild)
        {
            builtOnce = true;
            m_rebuildLayout = false;

            ImGuiID root = dockId;
            ImGui::DockBuilderRemoveNode(root);
            ImGui::DockBuilderAddNode(root, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(root, vp->WorkSize);

            ImGuiID top = ImGui::DockBuilderSplitNode(root, ImGuiDir_Up, 0.065f, nullptr, &root);
            ImGuiID left = ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.19f, nullptr, &root);
            ImGuiID right = ImGui::DockBuilderSplitNode(root, ImGuiDir_Right, 0.26f, nullptr, &root);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(root, ImGuiDir_Down, 0.28f, nullptr, &root);
            ImGuiID leftLower = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.55f, nullptr, &left);

            if (ImGuiDockNode* topNode = ImGui::DockBuilderGetNode(top))
            {
                topNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            }

            ImGui::DockBuilderDockWindow("Toolbar", top);
            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Course", leftLower);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Viewport", root);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Diagnostics", bottom);
            ImGui::DockBuilderFinish(dockId);
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Project...")) { NewProjectFlow(); }
                if (ImGui::MenuItem("Open Project...")) { OpenProjectFlow(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_hasProject)) { SaveProject(); }
                if (ImGui::MenuItem("Save Copy As...", nullptr, false, m_hasProject)) { SaveProjectAs(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Reload From Disk", nullptr, false, m_hasProject))
                {
                    OpenProject(m_project.Paths().Root());
                }
                if (ImGui::MenuItem("Exit")) { SetNeedsToBeClosed(true); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem(("Undo " + m_commands.UndoName()).c_str(), "Ctrl+Z", false, m_commands.CanUndo())) { m_commands.Undo(); }
                if (ImGui::MenuItem(("Redo " + m_commands.RedoName()).c_str(), "Ctrl+Y", false, m_commands.CanRedo())) { m_commands.Redo(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selection != 0)) { DuplicateSelected(); }
                if (ImGui::MenuItem("Delete", "Del", false, m_selection != 0)) { DeleteSelected(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add", m_hasProject && !m_play.IsPlaying()))
            {
                std::string group;
                for (const auto& entry : PrimitiveFactory::Catalog())
                {
                    if (entry.group != group)
                    {
                        group = entry.group;
                        ImGui::SeparatorText(group.c_str());
                    }
                    if (ImGui::MenuItem(entry.label.c_str()))
                    {
                        AddObjectFromDoc(PrimitiveFactory::Make(entry.key, m_camera.Position() + m_camera.Forward() * 6.0f),
                                         entry.label);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("ImGui Demo", nullptr, &m_showImGuiDemo);
                if (ImGui::MenuItem("Reset Camera")) { m_camera.Reset(); }
                if (ImGui::MenuItem("Reset Layout"))
                {
                    m_rebuildLayout = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About CourseMaker", nullptr, &m_openAboutModal);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            ImGui::TextColored(kCyan, "%s", m_hasProject ? m_project.name.c_str() : "no project");
            if (m_commands.IsDirty())
            {
                ImGui::TextColored(kWarm, "  *unsaved");
            }
            ImGui::EndMainMenuBar();
        }
    }

    void EditorApp::DrawToolbar()
    {
        if (!ImGui::Begin("Toolbar", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::End();
            return;
        }

        const bool playing = m_play.IsPlaying();
        if (!playing)
        {
            const bool canPlayDrill = m_hasProject && m_activeDrill >= 0 &&
                m_activeDrill < static_cast<int>(m_project.drills.size());
            const bool canPlayCourse = m_hasProject && !m_project.drills.empty();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.55f, 0.30f, 1.0f));
            ImGui::BeginDisabled(!canPlayDrill);
            if (ImGui::Button(" Play Drill "))
            {
                EnterPlay(false);
            }
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::BeginDisabled(!canPlayCourse);
            if (ImGui::Button(" Play Course "))
            {
                EnterPlay(true);
            }
            ImGui::EndDisabled();
        }
        else
        {
            const game::GameState state = m_director ? m_director->State() : game::GameState::Playing;
            const bool paused = state == game::GameState::Paused;

            ImGui::BeginDisabled(state == game::GameState::DrillIntro);
            if (ImGui::Button(paused ? " Resume " : " Pause "))
            {
                TogglePauseDirected();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(" Stop ")) { ExitPlay(); }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button(" Restart ")) { RestartPlay(); }
            ImGui::SameLine();

            std::string label;
            ImVec4 colour = kGreen;
            switch (state)
            {
            case game::GameState::DrillIntro:    label = "INTRO"; colour = kCyan; break;
            case game::GameState::Countdown:
            {
                const int n = m_director ? m_director->CountdownNumber() : 0;
                label = n > 0 ? std::to_string(n) + "..." : "GO!";
                colour = kWarm;
                break;
            }
            case game::GameState::Playing:       label = "PLAYING"; colour = kGreen; break;
            case game::GameState::Paused:        label = "PAUSED"; colour = kWarm; break;
            case game::GameState::DrillComplete: label = "DRILL COMPLETE"; colour = kCyan; break;
            case game::GameState::RunComplete:   label = "RESULTS"; colour = kViolet; break;
            default: label = "PLAYING"; break;
            }
            const double clock = m_director ? m_director->DrillTimeSeconds() : m_play.PlayTimeSeconds();
            ImGui::TextColored(colour, "%s  %.1fs", label.c_str(), clock);
            if (m_director && m_director->DrillCount() > 1)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("drill %zu/%zu", m_director->CurrentDrillIndex() + 1, m_director->DrillCount());
            }
        }

        ImGui::SameLine(0, 24);
        ImGui::BeginDisabled(playing);
        ImGui::Text("Gizmo:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Move", m_gizmoOperation == ImGuizmo::TRANSLATE)) m_gizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE)) m_gizmoOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE)) m_gizmoOperation = ImGuizmo::SCALE;
        ImGui::SameLine(0, 20);
        ImGui::Checkbox("Grid Snap", &m_gridSnap);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        ImGui::DragFloat("##grid", &m_gridSize, 0.05f, 0.05f, 5.0f, "%.2f m");
        ImGui::EndDisabled();

        ImGui::SameLine(0, 24);
        if (m_placement.Active())
        {
            ImGui::TextColored(kWarm, "PLACING %s  (click=confirm, Esc=cancel, wheel=distance, R=rotate)",
                m_placement.Label().c_str());
        }
        else if (!m_statusLine.empty())
        {
            ImGui::TextDisabled("%s", m_statusLine.c_str());
        }

        ImGui::End();
    }

    void EditorApp::DrawViewport()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        ImGui::PopStyleVar();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        m_viewportSize = glm::vec2(std::max(avail.x, 16.0f), std::max(avail.y, 16.0f));
        const ImVec2 imageMin = ImGui::GetCursorScreenPos();

        ImGui::Image((ImTextureID)(intptr_t)m_viewportFB.GetColorTexture(),
            ImVec2(m_viewportSize.x, m_viewportSize.y), ImVec2(0, 1), ImVec2(1, 0));

        m_viewportHovered = ImGui::IsItemHovered();
        m_viewportFocused = ImGui::IsWindowFocused();

        // Cursor position in NDC (y up).
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        m_viewportCursorNdc = glm::vec2(
            ((mouse.x - imageMin.x) / m_viewportSize.x) * 2.0f - 1.0f,
            1.0f - ((mouse.y - imageMin.y) / m_viewportSize.y) * 2.0f);

        const float aspect = m_viewportSize.x / m_viewportSize.y;

        // --- accept assets dropped from the browser -> start placement ---
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
            {
                const std::string rel(static_cast<const char*>(payload->Data), payload->DataSize);
                const AssetEntry* entry = m_assets.Find(rel);
                if (entry && entry->type == AssetType::Model)
                {
                    m_placement.Begin(m_scene.get(), PrimitiveFactory::MakeModelTarget(rel, glm::vec3(0.0f)), entry->name);
                }
                else if (entry && entry->type == AssetType::Image)
                {
                    m_placement.Begin(m_scene.get(), PrimitiveFactory::MakeImageTarget(rel, 1.0f, glm::vec3(0.0f)), entry->name);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // --- placement mode ------------------------------------------------
        if (m_placement.Active() && !m_play.IsPlaying())
        {
            PlacementTool::Frame frame;
            frame.ndcCursor = m_viewportCursorNdc;
            frame.aspect = aspect;
            frame.confirm = m_viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            frame.cancel = ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                (m_viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right));
            frame.wheel = m_viewportHovered ? ImGui::GetIO().MouseWheel : 0.0f;
            frame.rotateStep = m_viewportHovered && ImGui::IsKeyPressed(ImGuiKey_R);
            frame.gridSnap = m_gridSnap;
            frame.gridSize = m_gridSize;
            frame.alignToSurface = ImGui::GetIO().KeyShift;

            auto outcome = m_placement.Update(m_scene.get(), m_camera, frame);
            if (outcome.kind == PlacementTool::Outcome::Kind::Confirmed)
            {
                SceneEdit::Destroy(m_scene.get(), outcome.ghostId);
                auto command = std::make_unique<CreateObjectCommand>(m_scene.get(), 0, outcome.doc, "Place Object");
                auto* raw = command.get();
                m_commands.Execute(std::move(command));
                Select(raw->CreatedId());
                EditorLog::Success("placement", "Placed object.");
            }
            else if (outcome.kind == PlacementTool::Outcome::Kind::Cancelled)
            {
                SceneEdit::Destroy(m_scene.get(), outcome.ghostId);
                EditorLog::Info("placement", "Placement cancelled.");
            }
        }

        // --- gizmo -------------------------------------------------------
        eng::GameObject* selected = Selected();
        if (selected && !m_play.IsPlaying() && !m_placement.Active())
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(imageMin.x, imageMin.y, m_viewportSize.x, m_viewportSize.y);

            glm::mat4 view = m_activeCameraData.viewMatrix;
            glm::mat4 proj = m_activeCameraData.projectionMatrix;
            glm::mat4 world = selected->GetWorldTransform();

            static glm::vec3 dragStartP, dragStartS;
            static glm::quat dragStartR;
            const bool wasUsing = ImGuizmo::IsUsing();

            float snap[3] = { m_gridSize, m_gridSize, m_gridSize };
            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                    static_cast<ImGuizmo::OPERATION>(m_gizmoOperation), ImGuizmo::WORLD,
                    glm::value_ptr(world), nullptr, m_gridSnap ? snap : nullptr))
            {
                if (!wasUsing)
                {
                    dragStartP = selected->GetPosition();
                    dragStartR = selected->GetRotation();
                    dragStartS = selected->GetScale();
                }
                glm::vec3 t, s, skew;
                glm::vec4 persp;
                glm::quat r;
                glm::decompose(world, s, r, t, skew, persp);

                if (selected->GetParent())
                {
                    glm::mat4 parentInv = glm::inverse(selected->GetParent()->GetWorldTransform());
                    glm::vec3 lt, ls, lskew; glm::vec4 lpersp; glm::quat lr;
                    glm::decompose(parentInv * world, ls, lr, lt, lskew, lpersp);
                    selected->SetPosition(lt); selected->SetRotation(lr); selected->SetScale(ls);
                }
                else
                {
                    selected->SetPosition(t); selected->SetRotation(r); selected->SetScale(s);
                }
            }
            if (wasUsing && !ImGuizmo::IsUsing())
            {
                PushTransformEdit(selected, dragStartP, dragStartR, dragStartS);
            }
        }

        // --- click to select (analytic pick) ----------------------------
        if (m_viewportHovered && !m_play.IsPlaying() && !m_placement.Active() &&
            !ImGuizmo::IsOver() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            glm::vec3 ro, rd;
            m_camera.Ray(m_viewportCursorNdc, aspect, ro, rd);

            uint64_t best = 0;
            float bestT = 1e30f;
            std::function<void(eng::GameObject*)> visit = [&](eng::GameObject* obj)
            {
                if (!obj->IsActive()) return;
                if (obj->GetComponent<eng::MeshComponent>())
                {
                    const nlohmann::json doc = obj->Serialize();
                    const glm::vec3 he = MeshHalfExtents(doc);
                    const glm::mat4 inv = glm::inverse(obj->GetWorldTransform());
                    const glm::vec3 lo = glm::vec3(inv * glm::vec4(ro, 1.0f));
                    const glm::vec3 ld = glm::normalize(glm::vec3(inv * glm::vec4(rd, 0.0f)));
                    float t;
                    if (RayAabb(lo, ld, -he - glm::vec3(0.05f), he + glm::vec3(0.05f), t) && t < bestT)
                    {
                        bestT = t;
                        best = obj->GetId();
                    }
                }
                for (const auto& child : obj->GetChildren()) visit(child.get());
            };
            for (const auto& root : m_scene->GetRootObjects()) visit(root.get());
            Select(best);
        }

        ImGui::End();
    }

    void EditorApp::DrawHierarchy()
    {
        if (!ImGui::Begin("Hierarchy"))
        {
            ImGui::End();
            return;
        }

        if (!m_scene || m_scene->GetRootObjects().empty())
        {
            ImGui::TextDisabled("Scene is empty.");
            ImGui::TextDisabled("Use the Add menu to place content.");
            ImGui::End();
            return;
        }

        std::function<void(eng::GameObject*)> node = [&](eng::GameObject* obj)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (obj->GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
            if (obj->GetId() == m_selection) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::PushID(static_cast<int>(obj->GetId()));
            const bool open = ImGui::TreeNodeEx(obj->GetName().empty() ? "(unnamed)" : obj->GetName().c_str(), flags);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                Select(obj->GetId());
            }
            if (ImGui::BeginPopupContextItem())
            {
                Select(obj->GetId());
                if (ImGui::MenuItem("Duplicate")) DuplicateSelected();
                if (ImGui::MenuItem("Delete")) DeleteSelected();
                if (ImGui::MenuItem("Focus (F)")) FocusSelected();
                ImGui::EndPopup();
            }
            if (open)
            {
                for (const auto& child : obj->GetChildren())
                {
                    node(child.get());
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        };

        for (const auto& root : m_scene->GetRootObjects())
        {
            node(root.get());
        }

        ImGui::End();
    }

    void EditorApp::DrawInspector()
    {
        if (!ImGui::Begin("Inspector"))
        {
            ImGui::End();
            return;
        }

        eng::GameObject* obj = Selected();
        if (!obj)
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", obj->GetName().c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            nlohmann::json before = obj->Serialize();
            nlohmann::json after = before;
            after["name"] = nameBuf;
            m_commands.Execute(std::make_unique<ModifyObjectCommand>(m_scene.get(), obj->GetId(),
                before, after, "Rename"));
        }
        ImGui::TextDisabled("id %llu   type %s", static_cast<unsigned long long>(obj->GetId()), obj->GetTypeName());
        ImGui::Separator();

        // --- transform (direct setters + merged undo) ---
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec3 pos = obj->GetPosition();
            glm::vec3 scl = obj->GetScale();
            glm::quat q = obj->GetRotation();
            glm::vec3 euler = glm::degrees(glm::eulerAngles(q));

            static uint64_t editingId = 0;
            static glm::vec3 preP, preS; static glm::quat preR;

            auto beginEdit = [&]()
            {
                if (editingId != obj->GetId())
                {
                    editingId = obj->GetId();
                    preP = obj->GetPosition(); preR = obj->GetRotation(); preS = obj->GetScale();
                }
            };

            bool changed = false;
            if (ImGui::DragFloat3("Position", &pos.x, 0.05f)) { beginEdit(); obj->SetPosition(pos); changed = true; }
            if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f))
            {
                beginEdit();
                obj->SetRotation(glm::quat(glm::radians(euler)));
                changed = true;
            }
            if (ImGui::DragFloat3("Scale", &scl.x, 0.02f, 0.001f, 1000.0f)) { beginEdit(); obj->SetScale(scl); changed = true; }
            (void)changed;

            if (editingId == obj->GetId() && ImGui::IsItemDeactivatedAfterEdit())
            {
                PushTransformEdit(obj, preP, preR, preS);
                editingId = 0;
            }
        }

        // --- components ---
        nlohmann::json doc = obj->Serialize();
        bool docChanged = false;

        if (doc.contains("components") && doc["components"].is_array())
        {
            for (auto& comp : doc["components"])
            {
                const std::string type = comp.value("type", "?");
                if (!ImGui::CollapsingHeader(type.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    continue;
                }
                ImGui::PushID(type.c_str());

                if (type == "MeshComponent" && comp.contains("mesh"))
                {
                    auto& mesh = comp["mesh"];
                    std::string mtype = mesh.value("type", "box");
                    ImGui::TextDisabled("mesh: %s", mtype.c_str());
                    if (mtype == "box")
                    {
                        float x = mesh.value("x", 1.0f), y = mesh.value("y", 1.0f), z = mesh.value("z", 1.0f);
                        float v[3] = { x, y, z };
                        if (ImGui::DragFloat3("Size", v, 0.05f, 0.01f, 200.0f))
                        { mesh["x"] = v[0]; mesh["y"] = v[1]; mesh["z"] = v[2]; docChanged = true; }
                    }
                    else if (mtype == "sphere")
                    {
                        float r = mesh.value("r", 1.0f);
                        if (ImGui::DragFloat("Radius", &r, 0.02f, 0.01f, 100.0f)) { mesh["r"] = r; docChanged = true; }
                    }
                    else if (mtype == "quad" || mtype == "square")
                    {
                        float w = mesh.value("width", 1.0f), h = mesh.value("height", 1.0f);
                        float v[2] = { w, h };
                        if (ImGui::DragFloat2("W/H", v, 0.02f, 0.01f, 100.0f))
                        { mesh["width"] = v[0]; mesh["height"] = v[1]; docChanged = true; }
                    }
                    else if (mtype == "disc" || mtype == "circle" || mtype == "triangle" || mtype == "polygon" || mtype == "ngon")
                    {
                        float r = mesh.value("radius", 0.5f);
                        if (ImGui::DragFloat("Radius", &r, 0.02f, 0.01f, 100.0f)) { mesh["radius"] = r; docChanged = true; }
                    }

                    // Colour param on the material.
                    if (comp.contains("material") && comp["material"].contains("params") &&
                        comp["material"]["params"].contains("float3"))
                    {
                        for (auto& p : comp["material"]["params"]["float3"])
                        {
                            if (p.value("name", "") != "color") continue;
                            float col[3] = { p.value("value0", 1.0f), p.value("value1", 1.0f), p.value("value2", 1.0f) };
                            if (ImGui::ColorEdit3("Colour", col))
                            { p["value0"] = col[0]; p["value1"] = col[1]; p["value2"] = col[2]; docChanged = true; }
                        }
                    }
                }
                else if (type == "TargetComponent")
                {
                    const char* kinds[] = { "generic", "cube", "red", "moving" };
                    std::string kind = comp.value("kind", "generic");
                    int idx = 0;
                    for (int i = 0; i < 4; ++i) if (kind == kinds[i]) idx = i;
                    if (ImGui::Combo("Kind", &idx, kinds, 4)) { comp["kind"] = kinds[idx]; docChanged = true; }

                    char tag[64]; std::snprintf(tag, sizeof(tag), "%s", comp.value("tag", "").c_str());
                    if (ImGui::InputText("Tag", tag, sizeof(tag))) { comp["tag"] = tag; docChanged = true; }

                    float despawn = comp.value("despawnDelay", 1.4f);
                    if (ImGui::DragFloat("Despawn Delay", &despawn, 0.02f, 0.0f, 10.0f)) { comp["despawnDelay"] = despawn; docChanged = true; }

                    float impulse = comp.value("hitImpulse", 10.0f);
                    if (ImGui::DragFloat("Hit Impulse", &impulse, 0.5f, 0.0f, 200.0f)) { comp["hitImpulse"] = impulse; docChanged = true; }
                }
                else if (type == "MovingTargetComponent")
                {
                    glm::vec3 a = JsonVec3(comp, "pointA", glm::vec3(0.0f));
                    glm::vec3 b = JsonVec3(comp, "pointB", glm::vec3(0.0f));
                    float speed = comp.value("speed", 2.0f);
                    bool rel = comp.value("relative", true);
                    if (ImGui::DragFloat3("Point A", &a.x, 0.05f)) { comp["pointA"] = { {"x",a.x},{"y",a.y},{"z",a.z} }; docChanged = true; }
                    if (ImGui::DragFloat3("Point B", &b.x, 0.05f)) { comp["pointB"] = { {"x",b.x},{"y",b.y},{"z",b.z} }; docChanged = true; }
                    if (ImGui::DragFloat("Speed (m/s)", &speed, 0.05f, 0.0f, 50.0f)) { comp["speed"] = speed; docChanged = true; }
                    if (ImGui::Checkbox("Relative to spawn", &rel)) { comp["relative"] = rel; docChanged = true; }
                }
                else if (type == "LightComponent" && comp.contains("color"))
                {
                    auto& col = comp["color"];
                    float c[3] = { col.value("r", 1.0f), col.value("g", 1.0f), col.value("b", 1.0f) };
                    if (ImGui::ColorEdit3("Colour", c)) { col["r"] = c[0]; col["g"] = c[1]; col["b"] = c[2]; docChanged = true; }
                }
                else
                {
                    ImGui::TextDisabled("(no inline editor - edit via JSON)");
                }

                ImGui::PopID();
            }
        }

        if (docChanged)
        {
            nlohmann::json before = obj->Serialize();
            m_commands.Execute(std::make_unique<ModifyObjectCommand>(m_scene.get(), obj->GetId(),
                before, doc, "Edit " + obj->GetName(), "inspect:" + std::to_string(obj->GetId())));
        }

        ImGui::End();
    }

    void EditorApp::DrawAssetBrowser()
    {
        if (!ImGui::Begin("Assets"))
        {
            ImGui::End();
            return;
        }

        if (!m_hasProject)
        {
            ImGui::TextDisabled("Open a project to browse its assets.");
            ImGui::End();
            return;
        }

        if (ImGui::Button("Rescan")) { RefreshAssets(); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "search assets...", m_searchBuffer, sizeof(m_searchBuffer));

        static int typeFilter = 0;
        const char* filters[] = { "All", "Models", "Images", "Audio", "Materials", "Scenes", "Fonts" };
        ImGui::Combo("Type", &typeFilter, filters, IM_ARRAYSIZE(filters));
        const AssetType wanted =
            typeFilter == 1 ? AssetType::Model : typeFilter == 2 ? AssetType::Image :
            typeFilter == 3 ? AssetType::Audio : typeFilter == 4 ? AssetType::Material :
            typeFilter == 5 ? AssetType::Scene : typeFilter == 6 ? AssetType::Font : AssetType::Other;

        ImGui::Separator();
        if (ImGui::BeginChild("assetlist"))
        {
            for (const AssetEntry* entry : m_assets.Query(m_searchBuffer, wanted))
            {
                ImGui::PushID(entry->relativePath.c_str());
                const bool selectable = ImGui::Selectable(entry->name.c_str(), false,
                    ImGuiSelectableFlags_AllowDoubleClick);

                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("ASSET_PATH", entry->relativePath.c_str(), entry->relativePath.size());
                    ImGui::Text("%s", entry->name.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::SameLine();
                ImGui::TextColored(entry->missing ? kRed : ImVec4(0.55f, 0.58f, 0.62f, 1.0f),
                    "  %s%s", AssetLibrary::TypeLabel(entry->type), entry->missing ? "  (MISSING)" : "");

                if (selectable && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !m_play.IsPlaying())
                {
                    if (entry->type == AssetType::Model)
                        m_placement.Begin(m_scene.get(), PrimitiveFactory::MakeModelTarget(entry->relativePath, glm::vec3(0.0f)), entry->name);
                    else if (entry->type == AssetType::Image)
                        m_placement.Begin(m_scene.get(), PrimitiveFactory::MakeImageTarget(entry->relativePath, 1.0f, glm::vec3(0.0f)), entry->name);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void EditorApp::DrawCoursePanel()
    {
        if (!ImGui::Begin("Course"))
        {
            ImGui::End();
            return;
        }
        if (!m_hasProject)
        {
            ImGui::TextDisabled("No project open.");
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Course Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char nm[128]; std::snprintf(nm, sizeof(nm), "%s", m_project.name.c_str());
            if (ImGui::InputText("Name", nm, sizeof(nm))) { m_project.name = nm; m_commands.Push(std::make_unique<FunctionCommand>("rename project", []{}, []{})); }

            ImGui::SeparatorText("Movement defaults");
            auto& mv = m_project.movementDefaults;
            const auto presets = MovementSettings::PresetNames();
            int presetIdx = 0;
            for (size_t i = 0; i < presets.size(); ++i) if (presets[i] == mv.feelPreset) presetIdx = (int)i;
            if (ImGui::Combo("Feel", &presetIdx, [](void* d, int i)->const char*{ return (*(std::vector<std::string>*)d)[i].c_str(); },
                (void*)&presets, (int)presets.size()))
            {
                if (presets[presetIdx] != "custom") mv = MovementSettings::Preset(presets[presetIdx]);
                else mv.feelPreset = "custom";
            }
            ImGui::DragFloat("Move Speed (m/s)", &mv.moveSpeed, 0.1f, 0.5f, 30.0f);
            ImGui::DragFloat("Jump Height (m)", &mv.jumpHeight, 0.02f, 0.1f, 5.0f);
            ImGui::DragFloat("Ground Accel", &mv.groundAccel, 1.0f, 1.0f, 400.0f);
            ImGui::DragFloat("Ground Decel", &mv.groundDecel, 1.0f, 1.0f, 400.0f);
            ImGui::DragFloat("Friction", &mv.friction, 0.1f, 0.0f, 40.0f);
            ImGui::DragFloat("Air Control", &mv.airControl, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Mouse Sensitivity", &mv.mouseSensitivity, 0.02f, 0.1f, 20.0f);
            if (ImGui::Button("Reset movement to Responsive")) { mv = MovementSettings::Preset("responsive"); }

            ImGui::SeparatorText("Audio mix");
            ImGui::SliderFloat("Master", &m_project.masterVolume, 0.0f, 1.0f);
            ImGui::SliderFloat("Music", &m_project.musicVolume, 0.0f, 1.0f);
            ImGui::SliderFloat("Effects", &m_project.effectsVolume, 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Weapons", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (size_t i = 0; i < m_project.weapons.size(); ++i)
            {
                auto& w = m_project.weapons[i];
                ImGui::PushID((int)i);
                if (ImGui::TreeNode(w.name.c_str()))
                {
                    char nm[64]; std::snprintf(nm, sizeof(nm), "%s", w.name.c_str());
                    if (ImGui::InputText("Name", nm, sizeof(nm))) w.name = nm;

                    int cat = 0; const auto& cats = game::WeaponCatalog::Categories();
                    for (size_t k = 0; k < cats.size(); ++k) if (cats[k] == w.category) cat = (int)k;
                    if (ImGui::Combo("Category", &cat,
                        [](void* d, int i)->const char*{ static std::string s; s = game::WeaponCatalog::CategoryLabel((*(const std::vector<std::string>*)d)[i]); return s.c_str(); },
                        (void*)&cats, (int)cats.size())) w.category = cats[cat];

                    const char* modes[] = { "semi", "auto", "burst" };
                    int fm = 0; for (int k = 0; k < 3; ++k) if (w.fireMode == modes[k]) fm = k;
                    if (ImGui::Combo("Fire Mode", &fm, modes, 3)) w.fireMode = modes[fm];

                    ImGui::DragFloat("Fire Rate (rpm)", &w.fireRateRpm, 5.0f, 20.0f, 1400.0f);
                    ImGui::DragFloat("Damage", &w.damage, 1.0f, 1.0f, 500.0f);
                    ImGui::DragInt("Magazine", &w.magazine, 1, 1, 200);
                    ImGui::DragFloat("Reload (s)", &w.reloadSeconds, 0.05f, 0.1f, 10.0f);
                    ImGui::DragFloat("Spread (deg)", &w.spreadDegrees, 0.02f, 0.0f, 15.0f);
                    ImGui::DragFloat("Recoil", &w.recoil, 0.02f, 0.0f, 8.0f);
                    if (w.category == "sniper")
                        ImGui::DragFloat("Aim Zoom", &w.aimZoom, 0.1f, 1.0f, 12.0f);
                    ImGui::TextDisabled("shot interval: %.3f s", w.FireInterval());
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Key Bindings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto conflicts = m_project.bindings.Conflicts();
            for (auto& e : m_project.bindings.entries)
            {
                char buf[32]; std::snprintf(buf, sizeof(buf), "%s", e.second.c_str());
                const bool bad = std::find(conflicts.begin(), conflicts.end(), e.first) != conflicts.end();
                if (bad) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.15f, 0.15f, 1.0f));
                if (ImGui::InputText(e.first.c_str(), buf, sizeof(buf))) e.second = buf;
                if (bad) ImGui::PopStyleColor();
            }
            if (!conflicts.empty())
            {
                ImGui::TextColored(kRed, "Binding conflict - two actions share a key.");
            }
        }

        ImGui::SeparatorText("Drills");
        ImGui::TextDisabled("The active drill's targets are editable in the viewport.");
        if (ImGui::Button("+ Add Drill"))
        {
            FlushActiveDrill();
            m_project.AddDrill("");
            SelectDrill((int)m_project.drills.size() - 1);
        }

        int reorder = 0, reorderIdx = -1, deleteIdx = -1, dupIdx = -1;
        for (int i = 0; i < (int)m_project.drills.size(); ++i)
        {
            auto& d = m_project.drills[i];
            ImGui::PushID(i);
            const bool sel = (i == m_activeDrill);
            const std::string label = std::to_string(i + 1) + ".  " + d.name +
                (sel ? "   [editing]" : "");
            if (ImGui::Selectable(label.c_str(), sel) && !sel)
            {
                SelectDrill(i);
            }
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Move Up", nullptr, false, i > 0)) { reorder = -1; reorderIdx = i; }
                if (ImGui::MenuItem("Move Down", nullptr, false, i < (int)m_project.drills.size() - 1)) { reorder = 1; reorderIdx = i; }
                if (ImGui::MenuItem("Duplicate")) { dupIdx = i; }
                if (ImGui::MenuItem("Delete")) { deleteIdx = i; }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (reorderIdx >= 0)
        {
            FlushActiveDrill();
            if (m_project.MoveDrill(reorderIdx, reorder)) SelectDrill(reorderIdx + reorder);
        }
        if (dupIdx >= 0)
        {
            FlushActiveDrill();
            m_project.DuplicateDrill(dupIdx);
            SelectDrill(dupIdx + 1);
        }
        if (deleteIdx >= 0)
        {
            FlushActiveDrill();
            const bool wasActive = (deleteIdx == m_activeDrill);
            m_project.RemoveDrill(deleteIdx);
            if (wasActive || deleteIdx < m_activeDrill)
            {
                m_activeDrill = -1; // force reload
                SelectDrill(std::min(deleteIdx, (int)m_project.drills.size() - 1));
            }
        }

        if (m_activeDrill >= 0 && m_activeDrill < (int)m_project.drills.size())
        {
            auto& d = m_project.drills[m_activeDrill];
            ImGui::SeparatorText(("Drill: " + d.name).c_str());
            char nm[128]; std::snprintf(nm, sizeof(nm), "%s", d.name.c_str());
            if (ImGui::InputText("Drill Name", nm, sizeof(nm))) d.name = nm;
            char intro[256]; std::snprintf(intro, sizeof(intro), "%s", d.intro.c_str());
            if (ImGui::InputText("Intro line", intro, sizeof(intro))) d.intro = intro;
            ImGui::DragFloat("Par Time (s)", &d.targetTimeSeconds, 0.5f, 0.0f, 600.0f);
            ImGui::DragFloat3("Player Start", &d.playerStart.x, 0.1f);
            ImGui::Checkbox("Inherit project movement", &d.inheritMovement);
            char music[256]; std::snprintf(music, sizeof(music), "%s", d.musicPath.c_str());
            if (ImGui::InputTextWithHint("Music", "audio/track.wav (project-relative)", music, sizeof(music)))
                d.musicPath = music;

            ImGui::SeparatorText("Allowed Weapons");
            ImGui::TextDisabled("None checked = any project weapon; Play equips the first checked one.");
            for (const auto& w : m_project.weapons)
            {
                ImGui::PushID(w.id.c_str());
                bool allowed = std::find(d.allowedWeapons.begin(), d.allowedWeapons.end(), w.id)
                    != d.allowedWeapons.end();
                if (ImGui::Checkbox(w.name.c_str(), &allowed))
                {
                    if (allowed)
                    {
                        d.allowedWeapons.push_back(w.id);
                    }
                    else
                    {
                        d.allowedWeapons.erase(std::remove(d.allowedWeapons.begin(), d.allowedWeapons.end(), w.id),
                            d.allowedWeapons.end());
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", game::WeaponCatalog::CategoryLabel(w.category).c_str());
                ImGui::PopID();
            }

            ImGui::SeparatorText("Objectives");
            for (int k = 0; k < (int)d.objectives.size(); ++k)
            {
                auto& o = d.objectives[k];
                ImGui::PushID(1000 + k);
                const char* types[] = { "shoot", "knock", "reach", "finish" };
                int ti = 0; for (int t = 0; t < 4; ++t) if (o.type == types[t]) ti = t;
                ImGui::SetNextItemWidth(90);
                if (ImGui::Combo("##type", &ti, types, 4)) o.type = types[ti];
                ImGui::SameLine();
                if (o.type == "shoot" || o.type == "knock")
                {
                    ImGui::SetNextItemWidth(70);
                    ImGui::DragInt("x##amt", &o.amount, 0.1f, 1, 999);
                    ImGui::SameLine();
                }
                char desc[128]; std::snprintf(desc, sizeof(desc), "%s", o.description.c_str());
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputTextWithHint("##desc", "HUD text", desc, sizeof(desc))) o.description = desc;
                ImGui::SameLine();
                char tag[48]; std::snprintf(tag, sizeof(tag), "%s", o.tag.c_str());
                ImGui::SetNextItemWidth(70);
                if (ImGui::InputTextWithHint("##tag", "tag", tag, sizeof(tag))) o.tag = tag;
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) { d.objectives.erase(d.objectives.begin() + k); ImGui::PopID(); break; }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("+ Objective"))
            {
                d.objectives.push_back({ "shoot", "Hit the targets", 3, "", "" });
            }
        }

        ImGui::End();
    }

    void EditorApp::DrawDiagnostics()
    {
        if (!ImGui::Begin("Diagnostics"))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Warnings: %d   Errors: %d", EditorLog::WarningCount(), EditorLog::ErrorCount());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) { EditorLog::Clear(); }
        ImGui::SameLine();
        if (m_hasProject && ImGui::SmallButton("Revalidate"))
        {
            auto v = m_project.Validate();
            for (const auto& issue : v.issues)
            {
                const auto lvl = issue.level == ProjectValidation::Level::Error ? LogLevel::Error
                    : issue.level == ProjectValidation::Level::Warning ? LogLevel::Warning : LogLevel::Info;
                EditorLog::Add(lvl, "validation", issue.where + ": " + issue.message);
            }
        }
        ImGui::Separator();

        if (ImGui::BeginChild("loglist"))
        {
            for (const auto& entry : EditorLog::Entries())
            {
                ImGui::TextColored(LevelColour(entry.level), "[%7.1f] %-10s %s%s",
                    entry.time, entry.category.c_str(), entry.message.c_str(),
                    entry.count > 1 ? ("  (x" + std::to_string(entry.count) + ")").c_str() : "");
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void EditorApp::DrawModals()
    {
        if (m_openNewProjectModal)
        {
            ImGui::OpenPopup("New Project");
            m_openNewProjectModal = false;
        }
        if (m_openAboutModal)
        {
            ImGui::OpenPopup("About CourseMaker");
            m_openAboutModal = false;
        }

        if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Course name", m_newProjectName, sizeof(m_newProjectName));
            ImGui::InputText("Parent folder", m_newProjectPath, sizeof(m_newProjectPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                std::string f;
                if (PickFolderNative(f)) { std::snprintf(m_newProjectPath, sizeof(m_newProjectPath), "%s", f.c_str()); }
            }
            ImGui::Spacing();
            const bool canCreate = std::strlen(m_newProjectName) > 0 && std::strlen(m_newProjectPath) > 0;
            ImGui::BeginDisabled(!canCreate);
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                std::filesystem::path root = std::filesystem::path(m_newProjectPath) / m_newProjectName;
                CourseProject fresh;
                auto r = CourseProject::CreateNew(root, m_newProjectName, fresh);
                if (r.ok)
                {
                    SeedDefaultAssets(root);
                    ImGui::CloseCurrentPopup();
                    OpenProject(root);
                }
                else
                {
                    EditorLog::Error("project", "Create failed: " + r.error);
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("About CourseMaker", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Shooting Course Maker");
            ImGui::TextDisabled("A visual authoring tool for the GD-Engine.");
            ImGui::Separator();
            ImGui::BulletText("Dear ImGui (docking) + ImGuizmo for the editor UI");
            ImGui::BulletText("OpenGL / GLFW / GLEW / GLM renderer");
            ImGui::BulletText("Bullet physics, miniaudio, FreeType");
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
}
