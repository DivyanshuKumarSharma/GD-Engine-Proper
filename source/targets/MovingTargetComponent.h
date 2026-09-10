#pragma once

#include <eng.h>

#include <glm/vec3.hpp>

namespace game
{
    // Ping-pongs the owner between two points at a constant speed. Pair with a
    // kinematic PhysicsComponent so the collider (and therefore hitscan rays)
    // follow. Authored as:
    //
    //   { "type": "MovingTargetComponent", "pointA": {"x":0,"y":0,"z":0},
    //     "pointB": {"x":6,"y":0,"z":0}, "speed": 2.5, "relative": true }
    //
    // "relative" (default) treats the points as offsets from the spawn position.
    class MovingTargetComponent : public eng::Component
    {
        COMPONENT(MovingTargetComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void Init() override;
        void Update(float deltaTime) override;

    private:
        glm::vec3 m_pointA = glm::vec3(0.0f);
        glm::vec3 m_pointB = glm::vec3(0.0f);
        float m_speed = 2.0f;
        bool m_relative = true;

        glm::vec3 m_from = glm::vec3(0.0f);
        glm::vec3 m_to = glm::vec3(0.0f);
        float m_param = 0.0f;   // 0..1 along the segment
        int m_direction = 1;
    };
}
