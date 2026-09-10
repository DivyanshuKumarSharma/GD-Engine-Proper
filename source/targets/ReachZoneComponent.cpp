#include "targets/ReachZoneComponent.h"

namespace game
{
    void ReachZoneComponent::LoadProperties(const nlohmann::json& json)
    {
        m_tag = json.value("tag", "");
    }

    void ReachZoneComponent::Update(float /*deltaTime*/)
    {
        // Subscribe lazily so component order in the drill file doesn't matter.
        if (m_subscribed)
        {
            return;
        }

        if (auto* physics = m_owner->GetComponent<eng::PhysicsComponent>())
        {
            if (auto body = physics->GetRigidBody())
            {
                body->AddContactListener(this);
                m_subscribed = true;
            }
        }
    }

    void ReachZoneComponent::OnContact(eng::CollisionObject* other,
        const glm::vec3& /*position*/, const glm::vec3& /*normal*/)
    {
        if (m_fired || !other)
        {
            return;
        }

        if (other->GetCollisionObjectType() != eng::CollisionObjectType::KinematicCharacterController)
        {
            return;
        }

        m_fired = true;

        GameEvent event;
        event.type = GameEventType::ZoneReached;
        event.tag = m_tag;
        event.position = m_owner->GetWorldPosition();
        onGameEvent.Emit(event);
    }
}
