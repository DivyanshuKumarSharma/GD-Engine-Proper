#include "Engine.h"
#include "Application.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace eng
{
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        auto& inputManager = Engine::getInstance().GetInputManager();

        if (action == GLFW_PRESS)
        {
            inputManager.setKeyPressed(key, true);
        }
        else if (action == GLFW_RELEASE)
        {
            inputManager.setKeyPressed(key, false);
        }
    }
    Engine& Engine::getInstance()
    {
        static Engine instance; // Guaranteed to be destroyed and instantiated on first use
        return instance;
    }

    bool Engine::init(int width, int height)
    {
        // Initialization code here
        if(!m_application)
        {
            // No application set, cannot initialize
            return false;
        }

        if(glfwInit() != GLFW_TRUE)
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(width, height, "Game Window", nullptr, nullptr);

        if(m_window == nullptr)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwSetKeyCallback(m_window, keyCallback);

        glfwMakeContextCurrent(m_window);

        if(glewInit() != GLEW_OK)
        {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            glfwDestroyWindow(m_window);
            glfwTerminate();
            return false;
        }

        return m_application->Init();

    }

    void Engine::run()
    {
        // Main loop code here
        if(!m_application)
        {
            // No application set, cannot run
            return;
        }

        m_lastTimePoint = std::chrono::steady_clock::now();
        while (!glfwWindowShouldClose(m_window) && !m_application->needsToBeClosed())
        {
            glfwPollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(now - m_lastTimePoint).count();
            m_lastTimePoint = now;

            m_application->Update(deltaTime);

            glfwSwapBuffers(m_window);
        }
        
    }

    void Engine::Destroy()
    {
        // Cleanup code here
        if(m_application)
        {
            m_application->Destroy();
            m_application.reset();
            glfwTerminate();
            m_window = nullptr;
        }
    }

    void Engine::setApplication(Application* app)
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

    
}