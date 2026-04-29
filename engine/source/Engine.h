#pragma once
#include <memory>
#include <chrono>
#include "Input/InputManager.h"
#include "Graphics/GraphicsAPI.h"

struct GLFWwindow;
namespace eng
{
    class Application;
    class Engine
    {
    public:

        static Engine& getInstance();

    private:
        Engine() = default; // Private constructor for singleton pattern
        Engine(const Engine&) = delete; // Delete copy constructor
        Engine(Engine&&) = delete; // Delete move constructor
        Engine& operator=(const Engine&) = delete; // Delete copy assignment operator
        Engine& operator=(Engine&&) = delete; // Delete move assignment operator

    public:
        bool init(int width, int height);
        void run();
        void Destroy();

        void setApplication(Application* app);
        Application* GetApplication();
        InputManager& GetInputManager();
        GraphicsAPI& GetGraphicsAPI();

    private:
        std::unique_ptr<Application> m_application;
        std::chrono::steady_clock::time_point m_lastTimePoint;
        GLFWwindow* m_window = nullptr;
        InputManager m_inputManager;
        GraphicsAPI m_graphicsAPI;
    };
    
}