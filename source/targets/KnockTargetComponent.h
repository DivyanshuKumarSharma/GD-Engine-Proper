#pragma once

#include "course/GameEvent.h"
#include "util/Signal.h"

#include <eng.h>

#include <glm/vec3.hpp>

#include <string>

namespace game
{
    // Watches a dynamic physics prop and fires one ObjectKnocked event when it
    // satisfies a condition (fell off its platform, entered / left a marked box).
    // Authored as:
    //
    //   { "type": "KnockTargetComponent", "tag": "d2", "mode": "fallBelowY",
    //     "fallLineY": 1.2 }
    //   { "type": "KnockTargetComponent", "tag": "d3", "mode": "enterArea",
    //     "areaMin": {...}, "areaMax": {...} }
    class KnockTargetComponent : public eng::Component
    {
        COMPONENT(KnockTargetComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void Init() override;
        void Update(float deltaTime) override;

        Signal<GameEvent> onGameEvent;

        bool HasFired() const { return m_fired; }
        const std::string& Tag() const { return m_tag; }

        // Test hook: teleport the owner's body to a spot that satisfies the
        // trigger condition (safety net for the headless harness).
        void DebugForceCondition();

    private:
        enum class Mode { FallBelowY, EnterArea, LeaveArea };

        bool ConditionMet(const glm::vec3& position) const;

        Mode m_mode = Mode::FallBelowY;
        std::string m_tag;
        float m_fallLineY = 0.0f;
        glm::vec3 m_areaMin = glm::vec3(0.0f);
        glm::vec3 m_areaMax = glm::vec3(0.0f);
        float m_settleTime = 0.3f;   // condition must hold this long

        bool m_fired = false;
        float m_heldTime = 0.0f;
    };
}
