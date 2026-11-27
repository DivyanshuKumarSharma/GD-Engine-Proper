#pragma once
#include "Input/InputManager.h"
#include "Graphics/GraphicsAPI.h"
#include "graphics/Texture.h"
#include "render/RenderQueue.h"
#include "scene/Scene.h"
#include "io/FileSystem.h"
#include "physics/PhysicsManager.h"
#include "audio/AudioManager.h"
#include "font/FontManager.h"
#include "scene/components/ui/UIInputSystem.h"
#include "systems/JobSystem.h"

#include <memory>
#include <chrono>

struct GLFWwindow;
namespace eng
{
    class Application;
    class Framebuffer;
    class Engine
    {
    public:
        static Engine& GetInstance();

    private:
        Engine() = default;
        Engine(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine& operator=(Engine&&) = delete;

    public:
        bool Init(int width, int height);
        void Run();
        void Destroy();
        void SetCursorEnabled(bool enabled);

        void SetApplication(Application* app);
        Application* GetApplication();
        InputManager& GetInputManager();
        GraphicsAPI& GetGraphicsAPI();
        RenderQueue& GetRenderQueue();
        FileSystem& GetFileSystem();
        TextureManager& GetTextureManager();
        PhysicsManager& GetPhysicsManager();
        AudioManager& GetAudioManager();
        FontManager& GetFontManager();
        UIInputSystem& GetUIInputSystem();
        systems::JobSystem& GetJobs() { return m_jobs; }

        void SetScene(const std::shared_ptr<Scene>& scene);
        const std::shared_ptr<Scene>& GetScene();

        GLFWwindow* GetWindow() const { return m_window; }

        // --- Editor integration hooks (no effect unless a tool opts in) ---
        // When set, the scene is rendered into this offscreen target instead of
        // the window backbuffer (the editor then shows it inside a panel).
        void SetSceneRenderTarget(Framebuffer* target) { m_sceneRenderTarget = target; }
        Framebuffer* GetSceneRenderTarget() const { return m_sceneRenderTarget; }
        // When set, these camera matrices are used for the scene draw instead of
        // the scene's main camera (the editor supplies its free-fly camera).
        void SetCameraOverride(const CameraData* camera) { m_cameraOverride = camera; }

    private:
        systems::JobSystem m_jobs;
        bool m_applicationStarted = false;
        std::unique_ptr<Application> m_application;
        Framebuffer* m_sceneRenderTarget = nullptr;
        const CameraData* m_cameraOverride = nullptr;
        std::chrono::steady_clock::time_point m_lastTimePoint;
        GLFWwindow* m_window = nullptr;
        InputManager m_inputManager;
        GraphicsAPI m_graphicsAPI;
        RenderQueue m_rederQueue;
        FileSystem m_fileSystem;
        TextureManager m_textureManager;
        PhysicsManager m_physicsManager;
        AudioManager m_audioManager;
        FontManager m_fontManager;
        UIInputSystem m_uiInputSystem;
        std::shared_ptr<Scene> m_currentScene;
    };
}
