#pragma once

#include "targets/PathMotion.h"

#include <eng.h>

#include <glm/vec3.hpp>

#include <vector>

namespace game
{
    // Moves the owner along a waypoint path. Pair with a kinematic
    // PhysicsComponent so the collider (and therefore hitscan rays) follows.
    //
    //   { "type": "MovingTargetComponent",
    //     "points": [ {"x":-3,"y":0,"z":0}, {"x":3,"y":2,"z":0} ],
    //     "relative": true,          // points are offsets from the authored position
    //     "speed": 2.5,              // metres per second along the path
    //     "playback": "pingpong",    // pingpong | loop | once
    //     "easing": true,            // slow into each waypoint (segment time unchanged)
    //     "startDelay": 0.5,         // seconds before it starts moving
    //     "path": "diagonal" }       // editor preset label (informational)
    //
    // Legacy "pointA"/"pointB" are read as a two-point path.
    //
    // The authored transform is never touched until the first Update(), so a
    // scene loaded for editing (which never ticks components) keeps the exact
    // position the designer placed - no drift across save/reload.
    class MovingTargetComponent : public eng::Component
    {
        COMPONENT(MovingTargetComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void SaveProperties(nlohmann::json& json) const override;
        void Init() override;
        void Update(float deltaTime) override;

        const PathMotion& Motion() const { return m_motion; }
        // Authored waypoints: offsets from the owner's position when relative,
        // otherwise positions in the owner's parent space.
        const std::vector<glm::vec3>& Points() const { return m_points; }
        bool IsRelative() const { return m_relative; }

    private:
        void Rebuild();

        std::vector<glm::vec3> m_points;
        float m_speed = 2.0f;
        bool m_relative = true;
        bool m_easing = true;
        float m_startDelay = 0.0f;
        PathPlayback m_playback = PathPlayback::PingPong;

        PathMotion m_motion;
        glm::vec3 m_origin = glm::vec3(0.0f);
        bool m_originCaptured = false;
        nlohmann::json m_navigation;
    };
}
