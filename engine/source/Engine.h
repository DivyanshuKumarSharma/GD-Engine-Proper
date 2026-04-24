#pragma once
#include <memory>
#include <chrono>

namespace eng
{
    class Application;
    class Engine
    {
    public:
        bool init();
        void run();
        void Destroy();

        void setApplication(Application* app);
        Application* GetApplication();


    private:
        std::unique_ptr<Application> m_application;
        std::chrono::steady_clock::time_point m_lastTimePoint;
    };
    
}