#pragma once 

#include "scene/Component.h"
#include "physics/KinematicCharacterController.h"
#include <memory>
#include <string>

namespace eng
{
    class PlayerControllerComponent : public Component
    {
        COMPONENT(PlayerControllerComponent)

    public:
        void LoadProperties(const nlohmann::json& json) override;
        void SaveProperties(nlohmann::json& json) const override;
        void Init() override;
        void Update(float deltaTime) override;
        bool OnGround() const;

        // Independently gate look and movement input while keeping the collider in
        // the world. Menus disable both; a countdown disables movement only.
        void SetEnabled(bool enabled);          // both look and movement
        void SetLookEnabled(bool enabled);
        void SetMovementEnabled(bool enabled);
        bool IsLookEnabled() const;
        bool IsMovementEnabled() const;

        // Instantly move the player to a world position and stop any motion.
        void Teleport(const glm::vec3& worldPosition);

        // Nudge the view (degrees; +pitch looks up). Applied immediately, never
        // smoothed, so weapon recoil adds no input latency to aiming.
        void AddLookOffset(float pitchDegrees, float yawDegrees);

        // Frame-rate-independent horizontal-velocity integration used by the
        // "accel" movement model. Exposed for tests. `current`/`target` are m/s
        // in the ground plane; returns the new velocity after `dt` seconds.
        static glm::vec2 IntegrateGroundVelocity(
            const glm::vec2& current, const glm::vec2& target,
            bool hasInput, bool onGround,
            float accel, float decel, float friction, float airControl, float dt);

        // v = sqrt(2 * g * h) for a desired apex height in world units.
        static float JumpSpeedForHeight(float heightMetres, float gravity);

    private:
        bool m_lookEnabled = true;
        bool m_movementEnabled = true;
        float m_sensitivity = 15.0f;
        float m_moveSpeed = 50.0f;
        float m_jumpSpeed = 5.0f;
        float m_colliderRadius = 0.4f;
        float m_colliderHeight = 1.2f;

        // "accel" model (opt-in via "moveModel":"accel" or any accel key). When
        // legacy, movement stays the original instant Walk(dir * speed * dt).
        bool m_accelModel = false;
        float m_groundAccel = 90.0f;
        float m_groundDecel = 70.0f;
        float m_friction = 6.0f;
        float m_airControl = 0.35f;
        glm::vec2 m_velocity = glm::vec2(0.0f);
        float m_xRot = 0.0f;
        float m_yRot = 0.0f;
        std::string m_moveForwardAction = "MoveForward";
        std::string m_moveBackwardAction = "MoveBackward";
        std::string m_moveLeftAction = "MoveLeft";
        std::string m_moveRightAction = "MoveRight";
        std::string m_jumpAction = "Jump";
        std::unique_ptr<KinematicCharacterController> m_kinematicController;
    };
}
