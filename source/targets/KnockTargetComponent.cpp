#include "targets/KnockTargetComponent.h"

namespace game
{
    namespace
    {
        glm::vec3 ReadVec3(const nlohmann::json& parent, const char* key, const glm::vec3& fallback)
        {
            if (!parent.contains(key))
            {
                return fallback;
            }
            const auto& v = parent[key];
            return glm::vec3(v.value("x", fallback.x), v.value("y", fallback.y), v.value("z", fallback.z));
        }
    }

    void KnockTargetComponent::LoadProperties(const nlohmann::json& json)
    {
        const std::string mode = json.value("mode", "fallBelowY");
        if (mode == "enterArea")      m_mode = Mode::EnterArea;
        else if (mode == "leaveArea") m_mode = Mode::LeaveArea;
        else                          m_mode = Mode::FallBelowY;

        m_tag = json.value("tag", "");
        m_fallLineY = json.value("fallLineY", m_fallLineY);
        m_areaMin = ReadVec3(json, "areaMin", m_areaMin);
        m_areaMax = ReadVec3(json, "areaMax", m_areaMax);
        m_settleTime = json.value("settleTime", m_settleTime);
    }

    void KnockTargetComponent::Init()
    {
    }

    bool KnockTargetComponent::ConditionMet(const glm::vec3& position) const
    {
        const bool inside =
            position.x >= m_areaMin.x && position.x <= m_areaMax.x &&
            position.y >= m_areaMin.y && position.y <= m_areaMax.y &&
            position.z >= m_areaMin.z && position.z <= m_areaMax.z;

        switch (m_mode)
        {
        case Mode::FallBelowY: return position.y < m_fallLineY;
        case Mode::EnterArea:  return inside;
        case Mode::LeaveArea:  return !inside;
        }
        return false;
    }

    void KnockTargetComponent::DebugForceCondition()
    {
        auto* physics = m_owner->GetComponent<eng::PhysicsComponent>();
        if (!physics || !physics->GetRigidBody())
        {
            return;
        }

        glm::vec3 target = m_owner->GetWorldPosition();
        switch (m_mode)
        {
        case Mode::FallBelowY: target.y = m_fallLineY - 1.5f; break;
        case Mode::EnterArea:  target = (m_areaMin + m_areaMax) * 0.5f; break;
        case Mode::LeaveArea:  target = m_areaMin - glm::vec3(4.0f); break;
        }
        physics->GetRigidBody()->SetPosition(target);
    }

    void KnockTargetComponent::Update(float deltaTime)
    {
        if (m_fired)
        {
            return;
        }

        if (ConditionMet(m_owner->GetWorldPosition()))
        {
            m_heldTime += deltaTime;
            if (m_heldTime >= m_settleTime)
            {
                m_fired = true;

                GameEvent event;
                event.type = GameEventType::ObjectKnocked;
                event.tag = m_tag;
                event.position = m_owner->GetWorldPosition();
                onGameEvent.Emit(event);
            }
        }
        else
        {
            m_heldTime = 0.0f;
        }
    }
}
