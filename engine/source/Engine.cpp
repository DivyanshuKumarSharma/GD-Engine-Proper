#include "Engine.h"
#include "Application.h"

namespace eng
{
    bool Engine::init()
    {
        // Initialization code here
        if(!m_application)
        {
            // No application set, cannot initialize
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
        while (!m_application->needsToBeClosed())
        {
            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(now - m_lastTimePoint).count();
            m_lastTimePoint = now;

            m_application->Update(deltaTime);
        }
        
    }

    void Engine::Destroy()
    {
        // Cleanup code here
        if(m_application)
        {
            m_application->Destroy();
            m_application.reset();
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
}