#include "targets/PathMotion.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    const char* ToString(PathPlayback playback)
    {
        switch (playback)
        {
        case PathPlayback::Loop: return "loop";
        case PathPlayback::Once: return "once";
        default: return "pingpong";
        }
    }

    PathPlayback PathPlaybackFromString(const std::string& value)
    {
        if (value == "loop") return PathPlayback::Loop;
        if (value == "once" || value == "oneshot" || value == "one-shot") return PathPlayback::Once;
        return PathPlayback::PingPong;
    }

    void PathMotion::Configure(std::vector<glm::vec3> points, PathPlayback playback,
                               float speedMetresPerSecond, bool easing, float startDelaySeconds)
    {
        m_points = std::move(points);
        m_playback = playback;
        m_speed = std::max(0.0f, speedMetresPerSecond);
        m_easing = easing;
        m_startDelay = std::max(0.0f, startDelaySeconds);
        m_time = 0.0f;

        m_cumulative.assign(m_points.size(), 0.0f);
        m_length = 0.0f;
        for (std::size_t i = 1; i < m_points.size(); ++i)
        {
            m_length += glm::length(m_points[i] - m_points[i - 1]);
            m_cumulative[i] = m_length;
        }
        m_closingLength = m_points.size() >= 2 ? glm::length(m_points.front() - m_points.back()) : 0.0f;
    }

    float PathMotion::CycleSeconds() const
    {
        if (IsDegenerate())
        {
            return 0.0f;
        }
        switch (m_playback)
        {
        case PathPlayback::PingPong: return 2.0f * m_length / m_speed;
        case PathPlayback::Loop: return (m_length + m_closingLength) / m_speed;
        case PathPlayback::Once: return m_length / m_speed;
        }
        return 0.0f;
    }

    float PathMotion::PreviewPeriodSeconds() const
    {
        if (IsDegenerate())
        {
            return 1.0f;
        }
        const float hold = m_playback == PathPlayback::Once ? 1.0f : 0.0f;
        return m_startDelay + CycleSeconds() + hold;
    }

    bool PathMotion::IsFinished(float seconds) const
    {
        return m_playback == PathPlayback::Once &&
            (IsDegenerate() || seconds - m_startDelay >= m_length / m_speed);
    }

    glm::vec3 PathMotion::AtDistance(float distance, bool closed) const
    {
        const std::size_t n = m_points.size();
        const std::size_t segments = closed ? n : n - 1;
        float walked = 0.0f;
        for (std::size_t s = 0; s < segments; ++s)
        {
            const glm::vec3& a = m_points[s];
            const glm::vec3& b = m_points[(s + 1) % n];
            const float segLength = glm::length(b - a);
            if (segLength < 1e-6f)
            {
                continue; // duplicate waypoint: zero time spent here
            }
            if (distance <= walked + segLength || s + 1 == segments)
            {
                float f = std::clamp((distance - walked) / segLength, 0.0f, 1.0f);
                if (m_easing)
                {
                    f = f * f * (3.0f - 2.0f * f);
                }
                return glm::mix(a, b, f);
            }
            walked += segLength;
        }
        return closed ? m_points.front() : m_points.back();
    }

    glm::vec3 PathMotion::Sample(float seconds) const
    {
        if (m_points.empty())
        {
            return glm::vec3(0.0f);
        }
        const double t = static_cast<double>(seconds) - static_cast<double>(m_startDelay);
        if (t <= 0.0 || IsDegenerate())
        {
            return m_points.front();
        }

        // Double precision so a target left running for hours doesn't drift.
        double d = t * static_cast<double>(m_speed);
        switch (m_playback)
        {
        case PathPlayback::PingPong:
        {
            const double period = 2.0 * m_length;
            d = std::fmod(d, period);
            if (d > m_length)
            {
                d = period - d;
            }
            return AtDistance(static_cast<float>(d), false);
        }
        case PathPlayback::Loop:
        {
            const double total = static_cast<double>(m_length) + m_closingLength;
            d = std::fmod(d, total);
            return AtDistance(static_cast<float>(d), true);
        }
        case PathPlayback::Once:
            return AtDistance(static_cast<float>(std::min(d, static_cast<double>(m_length))), false);
        }
        return m_points.front();
    }

    glm::vec3 PathMotion::Advance(float deltaSeconds)
    {
        if (deltaSeconds > 0.0f)
        {
            m_time += deltaSeconds;
        }
        return Sample(m_time);
    }

    std::vector<glm::vec3> PathMotion::Preset(const std::string& name, float extent)
    {
        const float e = std::max(extent, 0.1f);
        if (name == "vertical") return { { 0.0f, 0.0f, 0.0f }, { 0.0f, e, 0.0f } };
        if (name == "diagonal") return { { -e, 0.0f, 0.0f }, { e, e, 0.0f } };
        if (name == "zigzag") return { { -e, 0.0f, 0.0f }, { -e * 0.33f, e * 0.6f, 0.0f },
                                       { e * 0.33f, 0.0f, 0.0f }, { e, e * 0.6f, 0.0f } };
        if (name == "box") return { { -e, 0.0f, 0.0f }, { e, 0.0f, 0.0f }, { e, e, 0.0f }, { -e, e, 0.0f } };
        return { { -e, 0.0f, 0.0f }, { e, 0.0f, 0.0f } }; // horizontal
    }
}
