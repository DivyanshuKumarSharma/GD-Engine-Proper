#include "scene/components/PlayerControllerComponent.h"
#include "input/InputManager.h"
#include "Engine.h"
#include "physics/PhysicsManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>
#include <algorithm>
#include <cmath>

namespace eng
{
    glm::vec2 PlayerControllerComponent::IntegrateGroundVelocity(
        const glm::vec2& current, const glm::vec2& target,
        bool hasInput, bool onGround,
        float accel, float decel, float friction, float airControl, float dt)
    {
        dt = std::min(std::max(dt, 0.0f), 0.1f); // large-frame guard
        glm::vec2 v = current;

        if (onGround && !hasInput && friction > 0.0f)
        {
            // Exponential damping - frame-rate independent.
            v *= std::exp(-friction * dt);
        }

        const float rate = hasInput
            ? (onGround ? accel : accel * airControl)
            : (onGround ? decel : 0.0f);

        if (rate > 0.0f)
        {
            glm::vec2 toTarget = target - v;
            const float dist = glm::length(toTarget);
            const float step = rate * dt;
            v += (dist > 1e-5f && step < dist) ? (toTarget / dist) * step : toTarget;
        }

        // Never let the accel model exceed the requested top speed on the ground.
        const float speed = glm::length(v);
        const float cap = glm::length(target);
        if (onGround && hasInput && cap > 1e-4f && speed > cap)
        {
            v *= cap / speed;
        }
        return v;
    }

    float PlayerControllerComponent::JumpSpeedForHeight(float heightMetres, float gravity)
    {
        const float g = std::fabs(gravity);
        return heightMetres > 0.0f && g > 0.0f ? std::sqrt(2.0f * g * heightMetres) : 0.0f;
    }

    void PlayerControllerComponent::LoadProperties(const nlohmann::json& json)
    {
        m_sensitivity = json.value("lookSensitivity", m_sensitivity);
        m_moveSpeed = json.value("moveSpeed", m_moveSpeed);
        m_jumpSpeed = json.value("jumpSpeed", m_jumpSpeed);

        const bool hasAccelKeys = json.contains("groundAccel") || json.contains("airControl") ||
            json.contains("groundDecel") || json.value("moveModel", "") == "accel";
        if (hasAccelKeys)
        {
            m_accelModel = true;
            m_groundAccel = json.value("groundAccel", m_groundAccel);
            m_groundDecel = json.value("groundDecel", m_groundDecel);
            m_friction = json.value("friction", m_friction);
            m_airControl = json.value("airControl", m_airControl);
        }
        if (json.contains("jumpHeight"))
        {
            const float gravity = 9.81f; // world gravity magnitude
            m_jumpSpeed = JumpSpeedForHeight(json.value("jumpHeight", 1.1f), gravity);
        }

        if (json.contains("collider"))
        {
            const auto& collider = json["collider"];
            m_colliderRadius = collider.value("radius", m_colliderRadius);
            m_colliderHeight = collider.value("height", m_colliderHeight);
        }

        if (json.contains("actions"))
        {
            const auto& actions = json["actions"];
            m_moveForwardAction = actions.value("forward", m_moveForwardAction);
            m_moveBackwardAction = actions.value("backward", m_moveBackwardAction);
            m_moveLeftAction = actions.value("left", m_moveLeftAction);
            m_moveRightAction = actions.value("right", m_moveRightAction);
            m_jumpAction = actions.value("jump", m_jumpAction);
        }
    }

    void PlayerControllerComponent::SaveProperties(nlohmann::json& json) const
    {
        json["lookSensitivity"] = m_sensitivity;
        json["moveSpeed"] = m_moveSpeed;
        json["jumpSpeed"] = m_jumpSpeed;
        if (m_accelModel)
        {
            json["moveModel"] = "accel";
            json["groundAccel"] = m_groundAccel;
            json["groundDecel"] = m_groundDecel;
            json["friction"] = m_friction;
            json["airControl"] = m_airControl;
        }
        json["collider"] = {
            { "radius", m_colliderRadius }, { "height", m_colliderHeight }
        };
        json["actions"] = {
            { "forward", m_moveForwardAction },
            { "backward", m_moveBackwardAction },
            { "left", m_moveLeftAction },
            { "right", m_moveRightAction },
            { "jump", m_jumpAction }
        };
    }

    void PlayerControllerComponent::Init()
    {
        m_kinematicController = std::make_unique<KinematicCharacterController>(
            m_colliderRadius, m_colliderHeight, m_owner->GetWorldPosition());
        m_kinematicController->SetOwnerObject(m_owner);
    }

    void PlayerControllerComponent::SetEnabled(bool enabled)
    {
        SetLookEnabled(enabled);
        SetMovementEnabled(enabled);
    }

    void PlayerControllerComponent::SetLookEnabled(bool enabled)
    {
        m_lookEnabled = enabled;
    }

    void PlayerControllerComponent::SetMovementEnabled(bool enabled)
    {
        m_movementEnabled = enabled;
        if (!enabled)
        {
            m_velocity = glm::vec2(0.0f);
            if (m_kinematicController)
            {
                m_kinematicController->Walk(glm::vec3(0.0f));
            }
        }
    }

    bool PlayerControllerComponent::IsLookEnabled() const
    {
        return m_lookEnabled;
    }

    bool PlayerControllerComponent::IsMovementEnabled() const
    {
        return m_movementEnabled;
    }

    void PlayerControllerComponent::Teleport(const glm::vec3& worldPosition)
    {
        if (!m_kinematicController)
        {
            return;
        }

        m_velocity = glm::vec2(0.0f);
        m_kinematicController->Walk(glm::vec3(0.0f));
        m_kinematicController->Warp(worldPosition);
        m_owner->SetPosition(m_kinematicController->GetPosition());
    }

    void PlayerControllerComponent::Update(float deltaTime)
    {
        auto& inputManager = Engine::GetInstance().GetInputManager();
        auto rotation = m_owner->GetRotation();

        if (m_lookEnabled && inputManager.IsMousePositionChanged())
        {
            const auto& oldPos = inputManager.GetMousePositionOld();
            const auto& currentPos = inputManager.GetMousePositionCurrent();

            float deltaX = currentPos.x - oldPos.x;
            float deltaY = currentPos.y - oldPos.y;

            // rot around Y axis
            float yDeltaAngle = -deltaX * m_sensitivity * deltaTime;
            m_yRot += yDeltaAngle;
            glm::quat yRot = glm::angleAxis(glm::radians(m_yRot), glm::vec3(0.0f, 1.0f, 0.0f));

            // rot around X axis
            float xDeltaAngle = -deltaY * m_sensitivity * deltaTime;
            m_xRot += xDeltaAngle;
            m_xRot = std::clamp(m_xRot, -89.0f, 89.0f);
            glm::quat xRot = glm::angleAxis(glm::radians(m_xRot), glm::vec3(1.0f, 0.0f, 0.0f));

            rotation = glm::normalize(yRot * xRot);

            m_owner->SetRotation(rotation);
        }

        glm::vec3 front = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);

        glm::vec3 move(0.0f);
        if (m_movementEnabled)
        {
            // Left/Right movement
            if (inputManager.IsActionDown(m_moveLeftAction))
            {
                move -= right;
            }
            else if (inputManager.IsActionDown(m_moveRightAction))
            {
                move += right;
            }
            // Vertical movement
            if (inputManager.IsActionDown(m_moveForwardAction))
            {
                move += front;
            }
            else if (inputManager.IsActionDown(m_moveBackwardAction))
            {
                move -= front;
            }

            if (inputManager.WasActionPressed(m_jumpAction))
            {
                m_kinematicController->Jump(glm::vec3(0.0f, m_jumpSpeed, 0.0f));
            }
        }

        if (m_accelModel)
        {
            glm::vec2 wish(0.0f);
            if (glm::dot(move, move) > 0.0f)
            {
                const glm::vec3 flat(move.x, 0.0f, move.z);
                if (glm::dot(flat, flat) > 1e-6f)
                {
                    wish = glm::normalize(glm::vec2(flat.x, flat.z));
                }
            }
            const glm::vec2 target = wish * m_moveSpeed;
            m_velocity = IntegrateGroundVelocity(m_velocity, target,
                glm::dot(wish, wish) > 0.0f, m_kinematicController->OnGround(),
                m_groundAccel, m_groundDecel, m_friction, m_airControl, deltaTime);
            m_kinematicController->Walk(glm::vec3(m_velocity.x, 0.0f, m_velocity.y) * deltaTime);
        }
        else
        {
            if (glm::dot(move, move) > 0)
            {
                move = glm::normalize(move);
            }
            m_kinematicController->Walk(move * m_moveSpeed * deltaTime);
        }

        m_owner->SetPosition(m_kinematicController->GetPosition());
    }

    bool PlayerControllerComponent::OnGround() const
    {
        if (m_kinematicController)
        {
            return m_kinematicController->OnGround();
        }
        return false;
    }
}
