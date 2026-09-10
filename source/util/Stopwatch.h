#pragma once

#include <chrono>
#include <string>

namespace game
{
    // Wall-clock stopwatch built on std::chrono::steady_clock. Time only advances
    // while running, so it is fully frame-rate independent and can be paused and
    // resumed (used for both the per-drill timer and the total run timer).
    class Stopwatch
    {
    public:
        void Reset();          // stop and zero the accumulated time
        void Start();          // start, or resume after Stop()
        void Stop();           // freeze; accumulated time is kept

        bool IsRunning() const { return m_running; }
        double ElapsedSeconds() const;

    private:
        using Clock = std::chrono::steady_clock;

        bool m_running = false;
        Clock::time_point m_startPoint{};
        double m_accumulatedSeconds = 0.0;
    };

    // Formats a duration as "MM:SS.cs" with two-digit centiseconds, e.g.
    // 62.371 -> "01:02.37". Negative inputs clamp to "00:00.00".
    std::string FormatStopwatch(double seconds);
}
