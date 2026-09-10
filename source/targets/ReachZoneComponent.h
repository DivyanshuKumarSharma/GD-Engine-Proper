#pragma once

#include "course/GameEvent.h"
#include "util/Signal.h"

#include <eng.h>

#include <string>

namespace game
{
    // A finish / checkpoint volume. Put it on an object with a trigger
    // PhysicsComponent ("body": { "type": "static", "trigger": true }); it fires
    // one ZoneReached event the first time the player walks into it.
    //
    //   { "type": "ReachZoneComponent", "tag": "finish" }
    class ReachZoneComponent : public eng::Component, public eng::IContactListener
    {
        COMPONENT(ReachZoneComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void Update(float deltaTime) override;

        void OnContact(eng::CollisionObject* other,
            const glm::vec3& position, const glm::vec3& normal) override;

        Signal<GameEvent> onGameEvent;

        bool HasFired() const { return m_fired; }
        const std::string& Tag() const { return m_tag; }

    private:
        std::string m_tag;
        bool m_subscribed = false;
        bool m_fired = false;
    };
}
