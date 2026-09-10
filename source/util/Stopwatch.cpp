#include "util/Stopwatch.h"

#include <cstdio>

namespace game
{
    void Stopwatch::Reset()
    {
        m_running = false;
        m_accumulatedSeconds = 0.0;
        m_startPoint = {};
    }

    void Stopwatch::Start()
    {
        if (m_running)
        {
            return;
        }
        m_running = true;
        m_startPoint = Clock::now();
    }

    void Stopwatch::Stop()
    {
        if (!m_running)
        {
            return;
        }
        m_accumulatedSeconds = ElapsedSeconds();
        m_running = false;
    }

    double Stopwatch::ElapsedSeconds() const
    {
        double elapsed = m_accumulatedSeconds;
        if (m_running)
        {
            const std::chrono::duration<double> delta = Clock::now() - m_startPoint;
            elapsed += delta.count();
        }
        return elapsed;
    }

    std::string FormatStopwatch(double seconds)
    {
        if (seconds < 0.0)
        {
            seconds = 0.0;
        }

        const int totalCentiseconds = static_cast<int>(seconds * 100.0 + 0.5);
        const int centiseconds = totalCentiseconds % 100;
        const int totalSeconds = totalCentiseconds / 100;
        const int minutes = totalSeconds / 60;
        const int wholeSeconds = totalSeconds % 60;

        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%02d", minutes, wholeSeconds, centiseconds);
        return std::string(buffer);
    }
}
