#pragma once

#include <eng.h>
#include "graphics/Framebuffer.h"

#include "editor/AssetLibrary.h"
#include "editor/CommandStack.h"
#include "editor/EditorCamera.h"
#include "editor/PlacementTool.h"
#include "editor/PlayController.h"
#include "project/CourseProject.h"
#include "systems/SpatialIndex.h"
#include <unordered_map>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace game
{
    class CourseDirector;
}

namespace editor
{
    using game::CourseProject;
    using game::DrillDoc;
    using game::MovementSettings;
    using game::ProjectValidation;
    using game::WeaponDef;

    // The CourseMaker editor: an eng::Application that renders the scene into an
    // offscreen target and drives a Dear ImGui docking UI on top of it.
    class EditorApp : public eng::Application
    {
    public:
        EditorApp();
        ~EditorApp() override;

        void SetStartupProject(std::filesystem::path path) { m_startupProject = std::move(path); }
        // Headless smoke / screenshot mode: run `frames` frames then quit,
        // writing PNGs into `shotsDir` (empty = no screenshots).
        void SetHeadless(int frames, std::string shotsDir)
        {
            m_headlessFrames = frames;
            m_shotsDir = std::move(shotsDir);
        }
        // Runs a scripted end-to-end workflow (add targets, undo/redo, save,
        // play, stop, verify restore) then quits with a PASS/FAIL log line.
        void SetDemo(bool demo) { m_demo = demo; }

        void RegisterTypes() override;
        bool Init() override;
        void Update(float deltaTime) override;
        void OnPostRender() override;
        void Destroy() override;

    private:
        // ---- project lifecycle ----
        void NewProjectFlow();
        void OpenProjectFlow();
        bool OpenProject(const std::filesystem::path& root);
        bool SaveProject();
        bool SaveProjectAs();
        void LoadEnvironmentScene();
        void RefreshAssets();
        // Copies the small set of default assets (checker material + textures +
        // shaders, placeholder weapon-tracer material, stock jump/shoot/step
        // sounds, a UI font) a brand new project needs to actually render/play,
        // from the assets CourseMaker ships with.
        void SeedDefaultAssets(const std::filesystem::path& projectRoot);

        // Drill "content layer": the selected drill's objects live under a
        // DrillRoot object inside the working scene so they are edited exactly
        // like the environment. SelectDrill flushes the current layer back to
        // its DrillDoc, then loads the new one.
        void SelectDrill(int index);
        void FlushActiveDrill();
        void LoadActiveDrillLayer();
        eng::GameObject* DrillRoot() const;

        // ---- editing ----
        void Select(uint64_t id);
        eng::GameObject* Selected() const;
        void AddObjectFromDoc(const nlohmann::json& doc, const std::string& label, uint64_t parentId = 0);
        void DeleteSelected();
        void DuplicateSelected();
        void FocusSelected();
        void PushTransformEdit(eng::GameObject* object,
                               const glm::vec3& oldP, const glm::quat& oldR, const glm::vec3& oldS);

        // ---- play ----
        // wholeCourse = false plays only the active drill (immediate individual
        // playtesting); true runs the full course start to finish.
        void EnterPlay(bool wholeCourse);
        void ExitPlay();
        void RestartPlay();
        // Pause/resume through the CourseDirector when one is running (matches
        // the standalone game's Esc behaviour and keeps world-frozen / cursor
        // state in sync); falls back to a plain physics pause otherwise.
        void TogglePauseDirected();
        // (Re)creates the CourseDirector against the current m_scene's Player and
        // begins it - used by EnterPlay and by RestartPlay (which rebuilds the
        // scene, so the director and its Player pointer must be rebuilt too).
        void BeginDirectorRun(bool wholeCourse);
        // The weapons the player spawns with: the active drill's Allowed Weapons
        // in order, or every project weapon when none are ticked (max 9 slots).
        std::vector<WeaponDef> ResolveLoadout() const;
        bool LoadProjectInputMap();
        void ApplyAudioMix();
        void PreviewSound(const std::string& projectRelativePath);
        std::string UniqueWeaponId(const std::string& base) const;
        // The binding token for whatever key / mouse button / wheel notch was
        // pressed this frame ("" if none, "Escape" to cancel a capture).
        static std::string CaptureBindingToken();

        // ---- panels ----
        void DrawDockSpaceAndMenu();
        void DrawToolbar();
        void DrawViewport();
        void DrawHierarchy();
        void DrawInspector();
        void DrawAssetBrowser();
        void DrawCoursePanel();
        void DrawDiagnostics();
        void DrawModals();

        void ApplyTheme();
        void HandleShortcuts();
        void RunDemo();

        // ---- state ----
        std::shared_ptr<eng::Scene> m_scene;
        eng::Framebuffer m_viewportFB;
        EditorCamera m_camera;
        eng::CameraData m_activeCameraData;
        CommandStack m_commands;
        PlayController m_play;
        PlacementTool m_placement;
        eng::systems::SpatialIndex m_pickIndex;
        struct PickShape { glm::mat4 inverse; glm::vec3 halfExtents; };
        std::unordered_map<uint64_t, PickShape> m_pickShapes;
        long m_pickRevision = -1;
        AssetLibrary m_assets;
        std::unique_ptr<game::CourseDirector> m_director;
        bool m_lastPlayWasWholeCourse = false;
        bool m_inputMapLoaded = false;

        CourseProject m_project;
        bool m_hasProject = false;
        std::filesystem::path m_startupProject;

        uint64_t m_selection = 0;
        int m_activeDrill = -1;

        // viewport
        glm::vec2 m_viewportSize = glm::vec2(1280.0f, 720.0f);
        glm::vec2 m_viewportCursorNdc = glm::vec2(0.0f);
        bool m_viewportHovered = false;
        bool m_viewportFocused = false;

        // tools
        int m_gizmoOperation = 7;  // ImGuizmo TRANSLATE
        bool m_gridSnap = true;
        float m_gridSize = 0.5f;
        bool m_showPaths = true;           // moving-target path preview overlay
        std::string m_bindingCapture;      // action whose key is being captured ("" = none)
        int m_bindingCaptureFrame = 0;
        std::shared_ptr<eng::Audio> m_previewSound;
        char m_searchBuffer[128] = { 0 };

        // modals
        bool m_openNewProjectModal = false;
        bool m_openAboutModal = false;
        bool m_confirmDiscardThenOpen = false;
        char m_newProjectName[128] = "My Course";
        char m_newProjectPath[512] = { 0 };
        std::string m_statusLine;
        double m_statusLineTime = 0.0;

        bool m_showImGuiDemo = false;
        bool m_rebuildLayout = false;
        double m_clock = 0.0;

        int m_headlessFrames = -1;
        int m_frame = 0;
        std::string m_shotsDir;

        bool m_demo = false;
        int m_demoStep = 0;
        int m_demoWait = 0;
        int m_demoPollFrames = 0;
        int m_demoFails = 0;
        uint64_t m_demoMoverId = 0;
        glm::vec3 m_demoMoverPos = glm::vec3(0.0f);
        bool m_demoAudio = false;
        nlohmann::json m_demoSnapshotCheck;
    };
}
