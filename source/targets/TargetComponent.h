#pragma once

#include "course/GameEvent.h"
#include "util/Signal.h"

#include <eng.h>

#include <glm/vec3.hpp>

#include <limits>
#include <string>

namespace game
{
    // Marks a scene object as a shootable target. Authored in a drill JSON file
    // next to a MeshComponent and (optionally) a PhysicsComponent:
    //
    //   { "type": "TargetComponent", "kind": "red", "tag": "d1", "hitImpulse": 12 }
    //
    // The ShootingController resolves a hitscan ray to the owning GameObject and
    // calls OnHit(). The component flashes, emits exactly one TargetShot event,
    // optionally shoves a dynamic body, then removes the owner after a short
    // window. Hitting an already-consumed target produces no further events.
    class TargetComponent : public eng::Component
    {
        COMPONENT(TargetComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void Init() override;
        void Update(float deltaTime) override;

        // Applies `damage` (default: lethal). Returns true only for the hit that
        // destroys the target - the one that emits the event. With "health" > 0
        // in the target's JSON it takes several hits (weapon damage matters);
        // with no health (default) any hit destroys it.
        bool OnHit(const glm::vec3& hitPoint, const glm::vec3& hitDirection,
                   float damage = std::numeric_limits<float>::infinity());

        bool IsConsumed() const { return m_consumed; }
        TargetKind Kind() const { return m_kind; }
        const std::string& Tag() const { return m_tag; }
        float Health() const { return m_health; }
        float MaxHealth() const { return m_maxHealth; }

        Signal<GameEvent> onGameEvent;

    private:
        TargetKind m_kind = TargetKind::Generic;
        std::string m_tag;
        float m_hitImpulse = 10.0f;     // knockback for a dynamic body
        float m_despawnDelay = 1.4f;    // time owner stays after being hit
        bool m_shrinkOnConsume = true;  // static plates shrink; flung cubes don't
        float m_maxHealth = 0.0f;       // 0 = one hit
        float m_health = 0.0f;

        bool m_consumed = false;
        float m_despawnTimer = 0.0f;
        float m_punch = 0.0f;           // non-lethal hit feedback timer
        glm::vec3 m_baseScale = glm::vec3(1.0f);
        eng::MeshComponent* m_mesh = nullptr;
        eng::PhysicsComponent* m_physics = nullptr;
    };
}
