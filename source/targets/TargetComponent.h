#pragma once

#include "course/GameEvent.h"
#include "util/Signal.h"

#include <eng.h>

#include <glm/vec3.hpp>

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

        // Returns true only for the first hit (the one that emits the event).
        bool OnHit(const glm::vec3& hitPoint, const glm::vec3& hitDirection);

        bool IsConsumed() const { return m_consumed; }
        TargetKind Kind() const { return m_kind; }
        const std::string& Tag() const { return m_tag; }

        Signal<GameEvent> onGameEvent;

    private:
        TargetKind m_kind = TargetKind::Generic;
        std::string m_tag;
        float m_hitImpulse = 10.0f;     // knockback for a dynamic body
        float m_despawnDelay = 1.4f;    // time owner stays after being hit
        bool m_shrinkOnConsume = true;  // static plates shrink; flung cubes don't

        bool m_consumed = false;
        float m_despawnTimer = 0.0f;
        glm::vec3 m_baseScale = glm::vec3(1.0f);
        eng::MeshComponent* m_mesh = nullptr;
        eng::PhysicsComponent* m_physics = nullptr;
    };
}
