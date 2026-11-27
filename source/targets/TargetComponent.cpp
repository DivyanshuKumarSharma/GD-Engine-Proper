#include "targets/TargetComponent.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr float kPunchSeconds = 0.12f;
        constexpr float kPunchScale = 0.15f;
    }

    void TargetComponent::LoadProperties(const nlohmann::json& json)
    {
        m_kind = TargetKindFromString(json.value("kind", "generic"));
        m_tag = json.value("tag", "");
        m_hitImpulse = json.value("hitImpulse", m_hitImpulse);
        m_despawnDelay = json.value("despawnDelay", m_despawnDelay);
        m_shrinkOnConsume = json.value("shrinkOnConsume", m_shrinkOnConsume);
        m_maxHealth = std::max(0.0f, json.value("health", 0.0f));
    }

    void TargetComponent::Init()
    {
        m_mesh = m_owner->GetComponent<eng::MeshComponent>();
        m_physics = m_owner->GetComponent<eng::PhysicsComponent>();
        m_baseScale = m_owner->GetScale();
        m_health = m_maxHealth;
    }

    void TargetComponent::Update(float deltaTime)
    {
        if (!m_consumed)
        {
            if (m_punch > 0.0f)
            {
                m_punch = std::max(0.0f, m_punch - deltaTime);
                m_owner->SetScale(m_baseScale * (1.0f + kPunchScale * (m_punch / kPunchSeconds)));
            }
            return;
        }

        m_despawnTimer += deltaTime;

        if (m_shrinkOnConsume && m_despawnDelay > 0.0f)
        {
            const float t = glm::clamp(m_despawnTimer / m_despawnDelay, 0.0f, 1.0f);
            m_owner->SetScale(m_baseScale * (1.0f - t));
        }

        if (m_despawnTimer >= m_despawnDelay)
        {
            m_owner->MarkForDestroy();
        }
    }

    bool TargetComponent::OnHit(const glm::vec3& hitPoint, const glm::vec3& hitDirection, float damage)
    {
        if (m_consumed)
        {
            return false;
        }

        glm::vec3 dir = hitDirection;
        if (glm::dot(dir, dir) > 0.0f)
        {
            dir = glm::normalize(dir);
        }

        if (m_maxHealth > 0.0f)
        {
            m_health -= std::max(0.0f, damage);
            if (m_health > 0.0f)
            {
                // Survived: a quick scale punch so the player sees the hit land.
                m_punch = kPunchSeconds;
                return false;
            }
            m_health = 0.0f;
        }

        m_consumed = true;
        m_despawnTimer = 0.0f;
        m_owner->SetScale(m_baseScale);

        // Visual: bright flash (the object is on its way out, no need to restore).
        if (m_mesh && m_mesh->GetMaterial())
        {
            m_mesh->GetMaterial()->SetParam("color", glm::vec3(1.0f, 1.0f, 1.0f));
        }

        // Physical: shove a dynamic body along the shot direction.
        if (m_physics && m_physics->GetRigidBody() &&
            m_physics->GetRigidBody()->GetType() == eng::BodyType::Dynamic)
        {
            m_physics->GetRigidBody()->ApplyImpulse(dir * m_hitImpulse);
            m_shrinkOnConsume = false; // let it tumble instead of shrinking
        }

        GameEvent event;
        event.type = GameEventType::TargetShot;
        event.targetKind = m_kind;
        event.tag = m_tag;
        event.position = hitPoint;
        onGameEvent.Emit(event);

        return true;
    }
}
