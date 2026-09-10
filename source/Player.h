#pragma once
#include <eng.h>
#include "PlayerConfig.h"

#include <glm/vec3.hpp>

class Player : public eng::GameObject
{
    GAMEOBJECT(Player)
public:
    void LoadProperties(const nlohmann::json& json) override;
    void Init() override;
    void Update(float deltaTime) override;

    // Course flow control (driven by CourseDirector):
    //   SetControlEnabled(false) - full freeze (menus, results, drill transitions)
    //   SetCombatEnabled(false)  - look only, no movement or firing (countdown)
    void SetControlEnabled(bool enabled);
    void SetCombatEnabled(bool enabled);

    void Teleport(const glm::vec3& worldPosition);

    // Pops the most recent shot as a camera-space ray for the ShootingController.
    bool ConsumeShotFired(glm::vec3& outOrigin, glm::vec3& outDirection);

    // Automation hook: queue a shot exactly as if the trigger had been pulled
    // (used by the headless self-test). Does not play the weapon FX.
    void InjectShot(const glm::vec3& origin, const glm::vec3& direction);

private:
    void FireProjectile();
    void ApplyControlFlags();

private:
    std::string m_configPath = "config/player.json";
    PlayerConfig m_config;
    float m_fireCooldown = 0.0f;

    bool m_controlEnabled = true;
    bool m_combatEnabled = true;

    bool m_shotPending = false;
    glm::vec3 m_shotOrigin = glm::vec3(0.0f);
    glm::vec3 m_shotDirection = glm::vec3(0.0f);

    eng::AnimationComponent* m_animationComponent = nullptr;
    eng::AudioComponent* m_audioComponent = nullptr;
    eng::PlayerControllerComponent* m_playerControllerComponent = nullptr;
};
