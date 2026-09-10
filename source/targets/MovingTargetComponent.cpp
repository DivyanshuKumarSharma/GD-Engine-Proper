#include "targets/MovingTargetComponent.h"
#include "targets/TargetComponent.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

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

    void MovingTargetComponent::LoadProperties(const nlohmann::json& json)
    {
        m_pointA = ReadVec3(json, "pointA", m_pointA);
        m_pointB = ReadVec3(json, "pointB", m_pointB);
        m_speed = json.value("speed", m_speed);
        m_relative = json.value("relative", m_relative);
    }

    void MovingTargetComponent::Init()
    {
        const glm::vec3 origin = m_relative ? m_owner->GetPosition() : glm::vec3(0.0f);
        m_from = origin + m_pointA;
        m_to = origin + m_pointB;
        m_param = 0.0f;
        m_direction = 1;
        m_owner->SetPosition(m_from);
    }

    void MovingTargetComponent::Update(float deltaTime)
    {
        if (auto* target = m_owner->GetComponent<TargetComponent>())
        {
            if (target->IsConsumed())
            {
                return;
            }
        }

        const float length = glm::length(m_to - m_from);
        if (length < 1e-4f)
        {
            return;
        }

        m_param += static_cast<float>(m_direction) * (m_speed / length) * deltaTime;
        if (m_param >= 1.0f)
        {
            m_param = 1.0f;
            m_direction = -1;
        }
        else if (m_param <= 0.0f)
        {
            m_param = 0.0f;
            m_direction = 1;
        }

        // Ease in/out so the reversal isn't a hard snap.
        const float smooth = m_param * m_param * (3.0f - 2.0f * m_param);
        m_owner->SetPosition(glm::mix(m_from, m_to, smooth));
    }
}
