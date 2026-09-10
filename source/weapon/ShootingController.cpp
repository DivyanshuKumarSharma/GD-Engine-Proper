#include "weapon/ShootingController.h"

#include "Player.h"
#include "stats/PlayerStats.h"
#include "targets/TargetComponent.h"

#include <eng.h>

#include <glm/geometric.hpp>

namespace game
{
    ShootingController::ShootingController(Player* player, PlayerStats* stats)
        : m_player(player)
        , m_stats(stats)
    {
    }

    void ShootingController::Update(float /*deltaTime*/)
    {
        if (!m_enabled || !m_player || !m_stats)
        {
            return;
        }

        glm::vec3 origin(0.0f);
        glm::vec3 direction(0.0f);
        if (m_player->ConsumeShotFired(origin, direction))
        {
            ResolveShot(origin, direction);
        }
    }

    void ShootingController::ResolveShot(const glm::vec3& origin, const glm::vec3& direction)
    {
        m_stats->RegisterShot();

        glm::vec3 unitDirection = direction;
        if (glm::dot(unitDirection, unitDirection) > 0.0f)
        {
            unitDirection = glm::normalize(unitDirection);
        }

        const glm::vec3 to = origin + unitDirection * m_range;
        const eng::RaycastHit hit =
            eng::Engine::GetInstance().GetPhysicsManager().Raycast(origin, to);

        if (!hit.hasHit || !hit.object || hit.object == static_cast<eng::GameObject*>(m_player))
        {
            return;
        }

        bool interactive = false;

        if (auto* physics = hit.object->GetComponent<eng::PhysicsComponent>())
        {
            if (auto body = physics->GetRigidBody();
                body && body->GetType() == eng::BodyType::Dynamic)
            {
                body->ApplyImpulse(unitDirection * m_knockbackImpulse);
                interactive = true;
            }
        }

        if (auto* target = hit.object->GetComponent<TargetComponent>())
        {
            target->OnHit(hit.point, unitDirection);
            interactive = true;
        }

        if (interactive)
        {
            m_stats->RegisterHit();
            onHitConfirmed.Emit(hit.point);
        }
    }
}
