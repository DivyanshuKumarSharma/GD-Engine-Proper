#pragma once
#include <eng.h>
#include "PlayerConfig.h"
#include "weapon/WeaponInventory.h"

#include <glm/vec3.hpp>

#include <deque>
#include <memory>
#include <vector>

class Player : public eng::GameObject
{
    GAMEOBJECT(Player)
public:
    // One authoritative trigger pull, resolved by the ShootingController's
    // hitscan. The physical/visual projectile never scores on its own.
    struct Shot
    {
        glm::vec3 origin = glm::vec3(0.0f);
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
        float damage = 1e9f;   // lethal unless a loadout weapon says otherwise
    };

    void LoadProperties(const nlohmann::json& json) override;
    void Init() override;
    void Update(float deltaTime) override;

    // Course flow control (driven by CourseDirector):
    //   SetControlEnabled(false) - full freeze (menus, results, drill transitions)
    //   SetCombatEnabled(false)  - look only, no movement or firing (countdown)
    void SetControlEnabled(bool enabled);
    void SetCombatEnabled(bool enabled);

    void Teleport(const glm::vec3& worldPosition);

    // Pops the oldest pending shot (several can be pending after one frame of
    // fast automatic fire).
    bool ConsumeShot(Shot& out);
    bool ConsumeShotFired(glm::vec3& outOrigin, glm::vec3& outDirection);

    // Automation hook: queue a shot exactly as if the trigger had been pulled
    // (used by the headless self-test). Does not play the weapon FX.
    void InjectShot(const glm::vec3& origin, const glm::vec3& direction);

    // Automation hook: behave as if Fire were held for `seconds` of game time,
    // through the real weapon logic (cadence, ammo, reload, spread, FX).
    void SimulateTrigger(float seconds) { m_simulatedTrigger = seconds; m_simulatedPress = true; }

    // Loadout (multi-weapon) mode. Null when the scene uses the single legacy
    // weapon from config/player.json.
    const game::WeaponInventory* Inventory() const { return m_useInventory ? &m_inventory : nullptr; }
    bool SelectWeapon(std::size_t index);
    bool IsAiming() const { return m_aiming; }

private:
    void FireProjectile();
    void ApplyControlFlags();
    void UpdateLoadout(float deltaTime, bool combatActive);
    void FireLoadoutRound(float bloom);
    void RefreshWeaponModels(float deltaTime);
    void PlayClip(const std::string& preferred, const std::string& fallback);
    void UpdateAimZoom(float deltaTime, bool aiming, float zoomFactor);

private:
    std::string m_configPath = "config/player.json";
    nlohmann::json m_weaponOverride = nlohmann::json::object();
    nlohmann::json m_loadoutJson = nlohmann::json::array();
    PlayerConfig m_config;
    float m_fireCooldown = 0.0f;

    bool m_controlEnabled = true;
    bool m_combatEnabled = true;

    std::deque<Shot> m_shots;

    // loadout mode
    bool m_useInventory = false;
    game::WeaponInventory m_inventory;
    std::vector<eng::GameObject*> m_weaponModels;   // "Weapon_<i>" children
    std::vector<glm::vec3> m_weaponModelRest;      // their authored local positions
    float m_modelKick = 0.0f;
    bool m_aiming = false;
    float m_simulatedTrigger = 0.0f;
    bool m_simulatedPress = false;
    std::shared_ptr<eng::Material> m_tracerMaterial;
    std::shared_ptr<eng::Mesh> m_tracerMesh;

    eng::AnimationComponent* m_animationComponent = nullptr;
    eng::AudioComponent* m_audioComponent = nullptr;
    eng::PlayerControllerComponent* m_playerControllerComponent = nullptr;
    eng::CameraComponent* m_cameraComponent = nullptr;
    float m_baseFov = 60.0f;
    float m_zoomBlend = 0.0f; // 0 = hip fire, 1 = fully aimed (eased, frame-rate independent)
    float m_zoomFactorInUse = 1.0f;
};
