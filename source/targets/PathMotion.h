#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace game
{
    enum class PathPlayback
    {
        PingPong, // A -> ... -> Z -> ... -> A, forever
        Loop,     // A -> ... -> Z -> A (closed circuit), forever
        Once      // A -> ... -> Z, then stop at Z
    };

    const char* ToString(PathPlayback playback);
    PathPlayback PathPlaybackFromString(const std::string& value);

    // Motion along a polyline of waypoints, evaluated as a pure function of
    // elapsed time - so any frame time (including a multi-second stall) lands on
    // exactly the position a fine-stepped simulation would, and an editor
    // preview can sample any moment without simulating.
    //
    // Speed semantics: `speed` is the travel speed in metres per second along
    // the path. Each segment therefore takes length / speed seconds. With
    // `easing` on, position inside a segment follows smoothstep: the segment
    // still takes exactly the same time (average speed unchanged), but the
    // target slows to a stop at every waypoint instead of snapping direction.
    class PathMotion
    {
    public:
        void Configure(std::vector<glm::vec3> points, PathPlayback playback,
                       float speedMetresPerSecond, bool easing, float startDelaySeconds);

        // Position at `seconds` after the motion started (including the delay).
        glm::vec3 Sample(float seconds) const;

        // Stateful convenience for runtime components.
        glm::vec3 Advance(float deltaSeconds);
        void Restart() { m_time = 0.0f; }
        float Time() const { return m_time; }

        float Length() const { return m_length; }   // open-path length (A..Z)
        // Seconds for one full cycle (PingPong: there and back; Loop: around;
        // Once: to the end). 0 for a degenerate path.
        float CycleSeconds() const;
        bool IsDegenerate() const { return m_points.size() < 2 || m_length < 1e-4f || m_speed <= 0.0f; }
        bool IsFinished(float seconds) const;
        const std::vector<glm::vec3>& Points() const { return m_points; }
        PathPlayback Playback() const { return m_playback; }
        float StartDelay() const { return m_startDelay; }
        // A preview period that shows one full cycle (plus the delay, and a
        // short hold at the end for Once) - what an editor animation loops over.
        float PreviewPeriodSeconds() const;

        // Preset point lists (relative to the target's authored position).
        static std::vector<glm::vec3> Preset(const std::string& name, float extent);

    private:
        glm::vec3 AtDistance(float distance, bool closed) const;

        std::vector<glm::vec3> m_points;
        std::vector<float> m_cumulative;   // distance at each point (open path)
        float m_length = 0.0f;             // open length
        float m_closingLength = 0.0f;      // last -> first (Loop)
        PathPlayback m_playback = PathPlayback::PingPong;
        float m_speed = 2.0f;
        bool m_easing = true;
        float m_startDelay = 0.0f;
        float m_time = 0.0f;
    };
}
