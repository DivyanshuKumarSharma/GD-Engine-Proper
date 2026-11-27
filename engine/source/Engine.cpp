#include "Engine.h"
#include "Application.h"
#include "graphics/Framebuffer.h"
#include "scene/GameObject.h"
#include "scene/Component.h"
#include "scene/components/CameraComponent.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#include "systems/Profiler.h"
#include "systems/FrameRecorder.h"

namespace eng
{
    void keyCallback(GLFWwindow* window, int key, int, int action, int)
    {
        auto& inputManager = eng::Engine::GetInstance().GetInputManager();
        if (action == GLFW_PRESS)
        {
            inputManager.SetKeyPressed(key, true);
        }
        else if (action == GLFW_RELEASE)
        {
            inputManager.SetKeyPressed(key, false);
        }
    }

    void mouseButtonCallback(GLFWwindow* window, int button, int action, int)
    {
        auto& inputManager = eng::Engine::GetInstance().GetInputManager();
        if (action == GLFW_PRESS)
        {
            inputManager.SetMouseButtonPressed(button, true);
        }
        else if (action == GLFW_RELEASE)
        {
            inputManager.SetMouseButtonPressed(button, false);
        }
    }

    void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
    {
        auto& inputManager = eng::Engine::GetInstance().GetInputManager();

        inputManager.SetMousePositionOld(inputManager.GetMousePositionCurrent());

        glm::vec2 currentPos(static_cast<float>(xpos), static_cast<float>(ypos));
        inputManager.SetMousePositionCurrent(currentPos);

        inputManager.SetMousePositionChanged(true);
    }

    void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset)
    {
        eng::Engine::GetInstance().GetInputManager().AddScroll(static_cast<float>(yoffset));
    }

    void windowSizeCallback(GLFWwindow* window, int width, int height)
    {
        eng::Engine::GetInstance().GetGraphicsAPI().SetViewport(0, 0, width, height);
    }

    Engine& Engine::GetInstance()
    {
        static Engine instance;
        return instance;
    }

    bool Engine::Init(int width, int height)
    {
        if (!m_application)
        {
            return false;
        }

        Scene::RegisterTypes();
        m_application->RegisterTypes();

#if defined (__linux__)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(width, height, "GameDevelopmentProject", nullptr, nullptr);

        if (m_window == nullptr)
        {
            std::cout << "Error creating window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwSetKeyCallback(m_window, keyCallback);
        glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
        glfwSetCursorPosCallback(m_window, cursorPositionCallback);
        glfwSetScrollCallback(m_window, scrollCallback);
        glfwSetWindowSizeCallback(m_window, windowSizeCallback);

        glfwMakeContextCurrent(m_window);

        if (glewInit() != GLEW_OK)
        {
            glfwTerminate();
            return false;
        }

        m_graphicsAPI.Init();
        m_graphicsAPI.SetViewport(0, 0, width, height);
        m_physicsManager.Init();
        m_audioManager.Init();
        m_rederQueue.Init();
        m_fontManager.Init();
        m_applicationStarted = true;
        return m_application->Init();
    }

    void Engine::Run()
    {
        if (!m_application)
        {
            return;
        }

        m_lastTimePoint = std::chrono::steady_clock::now();
        systems::FrameRecorder frames;
        const char* capturePath = std::getenv("GD_FRAME_CSV");
        const char* limitText = std::getenv("GD_FRAME_LIMIT");
        const int frameLimit = limitText ? std::max(0, std::atoi(limitText)) : 0;
        int frameCount = 0;

        while (!glfwWindowShouldClose(m_window) && !m_application->NeedsToBeClosed())
        {
            GD_ZONE("Engine frame");
            const auto frameStart = std::chrono::steady_clock::now();
            glfwPollEvents();

            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(now - m_lastTimePoint).count();
            m_lastTimePoint = now;

            m_physicsManager.Update(deltaTime);

            if (m_uiInputSystem.IsActive())
            {
                m_uiInputSystem.Update(deltaTime);
            }

            { GD_ZONE("Application update"); m_application->Update(deltaTime); }

            int width = 0;
            int height = 0;
            glfwGetWindowSize(m_window, &width, &height);

            const bool usingRenderTarget = m_sceneRenderTarget && m_sceneRenderTarget->IsValid();
            if (usingRenderTarget)
            {
                m_sceneRenderTarget->Bind();
            }

            m_graphicsAPI.ClearBuffers();

            CameraData cameraData;
            std::vector<LightData> lights;

            const int renderWidth = usingRenderTarget ? m_sceneRenderTarget->GetWidth() : width;
            const int renderHeight = usingRenderTarget ? m_sceneRenderTarget->GetHeight() : height;
            const float aspect = renderHeight > 0
                ? static_cast<float>(renderWidth) / static_cast<float>(renderHeight)
                : 1.0f;

            if (m_cameraOverride)
            {
                cameraData = *m_cameraOverride;
                cameraData.orthoMatrix = glm::ortho(
                    0.0f, static_cast<float>(renderWidth),
                    0.0f, static_cast<float>(renderHeight));
            }
            else if (m_currentScene)
            {
                if (auto cameraObject = m_currentScene->GetMainCamera())
                {
                    // logic for matrices
                    auto cameraComponent = cameraObject->GetComponent<CameraComponent>();
                    if (cameraComponent)
                    {
                        cameraData.viewMatrix = cameraComponent->GetViewMatrix();
                        cameraData.projectionMatrix = cameraComponent->GetProjectionMatrix(aspect);
                        cameraData.orthoMatrix = glm::ortho(
                            0.0f, static_cast<float>(renderWidth),
                            0.0f, static_cast<float>(renderHeight)
                        );
                        cameraData.position = cameraObject->GetWorldPosition();
                    }
                }
            }

            if (m_currentScene)
            {
                lights = m_currentScene->CollectLights();
            }

            { GD_ZONE("Scene render"); m_rederQueue.Draw(m_graphicsAPI, cameraData, lights); }

            if (usingRenderTarget)
            {
                Framebuffer::Unbind();
                m_graphicsAPI.SetViewport(0, 0, width, height);
            }

            m_application->OnPostRender();

            glfwSwapBuffers(m_window);

            m_inputManager.ClearStates();
            if (capturePath) frames.Add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count());
            GD_FRAME();
            if (frameLimit > 0 && ++frameCount >= frameLimit) break;
        }
        if (capturePath) {
            try { frames.Write(capturePath); }
            catch (const std::exception& e) { std::cerr << e.what() << std::endl; }
        }
    }

    void Engine::Destroy()
    {
        if (m_application)
        {
            m_jobs.WaitIdle();
            if (m_applicationStarted) m_application->Destroy();
            m_applicationStarted = false;
            m_application.reset();
            m_currentScene.reset();
            m_textureManager.Clear();
            m_fontManager.Clear();
            glfwTerminate();
            m_window = nullptr;
        }
    }

    void Engine::SetCursorEnabled(bool enabled)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    void Engine::SetApplication(Application* app)
    {
        m_application.reset(app);
    }

    Application* Engine::GetApplication()
    {
        return m_application.get();
    }

    InputManager& Engine::GetInputManager()
    {
        return m_inputManager;
    }

    GraphicsAPI& Engine::GetGraphicsAPI()
    {
        return m_graphicsAPI;
    }

    RenderQueue& Engine::GetRenderQueue()
    {
        return m_rederQueue;
    }

    FileSystem& Engine::GetFileSystem()
    {
        return m_fileSystem;
    }

    TextureManager& Engine::GetTextureManager()
    {
        return m_textureManager;
    }

    PhysicsManager& Engine::GetPhysicsManager()
    {
        return m_physicsManager;
    }

    AudioManager& Engine::GetAudioManager()
    {
        return m_audioManager;
    }

    FontManager& Engine::GetFontManager()
    {
        return m_fontManager;
    }

    UIInputSystem& Engine::GetUIInputSystem()
    {
        return m_uiInputSystem;
    }

    void Engine::SetScene(const std::shared_ptr<Scene>& scene)
    {
        m_currentScene = scene;
    }

    const std::shared_ptr<Scene>& Engine::GetScene()
    {
        return m_currentScene;
    }
}
