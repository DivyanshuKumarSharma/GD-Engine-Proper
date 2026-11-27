#include "Player.h"
#include "Bullet.h"
#include "weapon/Tracer.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
    constexpr float kTracerSpeed = 180.0f;       // m/s, visual only
    constexpr float kTracerLifetime = 0.12f;
    constexpr float kDrawDrop = 0.18f;           // model dips this far while drawing
    constexpr float kReloadDip = 0.12f;          // animation-less reload fallback
    constexpr float kModelKickDistance = 0.06f;
    constexpr float kModelKickRecovery = 6.0f;   // per second
    constexpr int kMaxWeaponSlots = 9;
}

void Player::LoadProperties(const nlohmann::json& json)
{
    m_configPath = json.value("config", m_configPath);
    // A per-play weapon override baked into the scene JSON (single-weapon mode).
    if (json.contains("weaponOverride") && json["weaponOverride"].is_object())
    {
        m_weaponOverride = json["weaponOverride"];
    }
    // A full loadout (multi-weapon mode): WeaponDef documents, with matching
    // "Weapon_<i>" model children. Baked in by the editor / course player.
    if (json.contains("loadout") && json["loadout"].is_array())
    {
        m_loadoutJson = json["loadout"];
    }
}

void Player::Init()
{
    std::string configError;
    if (!PlayerConfig::Load(m_configPath, m_config, &configError))
    {
        std::cerr << "Failed to load player configuration: " << configError << std::endl;
    }
    if (!m_weaponOverride.empty())
    {
        m_config.weapon.ApplyOverrides(m_weaponOverride);
    }

    for (const auto& objectName : m_config.weapon.hiddenObjectNames)
    {
        if (auto object = FindChildByName(objectName))
        {
            object->SetActive(false);
        }
    }

    if (auto gun = FindChildByName(m_config.weapon.weaponObjectName))
    {
        m_animationComponent = gun->GetComponent<eng::AnimationComponent>();
    }

    m_audioComponent = GetComponent<eng::AudioComponent>();
    m_playerControllerComponent = GetComponent<eng::PlayerControllerComponent>();
    m_cameraComponent = GetComponent<eng::CameraComponent>();
    if (m_cameraComponent)
    {
        m_baseFov = m_cameraComponent->GetFov();
    }

    if (!m_loadoutJson.empty())
    {
        std::vector<game::WeaponDef> weapons;
        for (const auto& entry : m_loadoutJson)
        {
            if (entry.is_object() && static_cast<int>(weapons.size()) < kMaxWeaponSlots)
            {
                weapons.push_back(game::WeaponDef::FromJson(entry));
            }
        }
        if (!weapons.empty())
        {
            m_useInventory = true;
            m_inventory.SetLoadout(std::move(weapons));
            m_inventory.Seed(static_cast<std::uint32_t>(GetId() * 2654435761u));
            for (std::size_t i = 0; i < m_inventory.Count(); ++i)
            {
                eng::GameObject* model = FindChildByName("Weapon_" + std::to_string(i));
                m_weaponModels.push_back(model);
                m_weaponModelRest.push_back(model ? model->GetPosition() : glm::vec3(0.0f));
            }
            m_tracerMesh = eng::Mesh::CreateBox(glm::vec3(0.022f, 0.022f, 0.9f));
            m_tracerMaterial = eng::Material::Load("materials/checker.mat");
            if (m_tracerMaterial)
            {
                m_tracerMaterial->SetParam("color", glm::vec3(1.0f, 0.86f, 0.45f));
            }
            RefreshWeaponModels(0.0f);
        }
    }

    ApplyControlFlags();
}

void Player::Update(float deltaTime)
{
    eng::GameObject::Update(deltaTime);
    m_fireCooldown = std::max(0.0f, m_fireCooldown - deltaTime);

    auto& input = eng::Engine::GetInstance().GetInputManager();
    const bool combatActive = m_controlEnabled && m_combatEnabled;

    if (m_useInventory)
    {
        UpdateLoadout(deltaTime, combatActive);
    }
    else
    {
        const bool wantsToFire = m_config.weapon.semiAuto
            ? input.WasActionPressed(m_config.weapon.fireAction)
            : input.IsActionDown(m_config.weapon.fireAction);

        if (combatActive && wantsToFire && m_fireCooldown <= 0.0f)
        {
            FireProjectile();
            m_fireCooldown = m_config.weapon.fireInterval;
        }
    }

    if (combatActive && input.WasActionPressed(m_config.jumpAction))
    {
        if (m_audioComponent && !m_audioComponent->IsPlaying(m_config.jumpAudioName))
        {
            m_audioComponent->Play(m_config.jumpAudioName);
        }
    }

    const bool walking = combatActive &&
        (input.IsActionDown(m_config.moveForwardAction) ||
         input.IsActionDown(m_config.moveBackwardAction) ||
         input.IsActionDown(m_config.moveLeftAction) ||
         input.IsActionDown(m_config.moveRightAction));

    if (walking && m_playerControllerComponent && m_playerControllerComponent->OnGround())
    {
        if (m_audioComponent && !m_audioComponent->IsPlaying(m_config.footstepsAudioName))
        {
            m_audioComponent->Play(m_config.footstepsAudioName, true);
        }
    }
    else
    {
        if (m_audioComponent && m_audioComponent->IsPlaying(m_config.footstepsAudioName))
        {
            m_audioComponent->Stop(m_config.footstepsAudioName);
        }
    }

    if (!m_useInventory)
    {
        const bool aiming = combatActive && input.IsActionDown("AimDownSights");
        UpdateAimZoom(deltaTime, aiming, m_config.weapon.aimZoomFactor);
    }
}

void Player::UpdateLoadout(float deltaTime, bool combatActive)
{
    auto& input = eng::Engine::GetInstance().GetInputManager();

    if (combatActive)
    {
        for (int i = 0; i < kMaxWeaponSlots; ++i)
        {
            if (input.WasActionPressed("WeaponSlot" + std::to_string(i + 1)))
            {
                SelectWeapon(static_cast<std::size_t>(i));
            }
        }
        if (input.WasActionPressed("WeaponNext"))
        {
            m_inventory.SelectRelative(+1);
        }
        if (input.WasActionPressed("WeaponPrev"))
        {
            m_inventory.SelectRelative(-1);
        }
        if (input.WasActionPressed("Reload"))
        {
            m_inventory.BeginReload();
        }
    }

    bool held = combatActive && input.IsActionDown(m_config.weapon.fireAction);
    bool pressed = combatActive && input.WasActionPressed(m_config.weapon.fireAction);
    if (combatActive && m_simulatedTrigger > 0.0f)
    {
        held = true;
        pressed = pressed || m_simulatedPress;
        m_simulatedPress = false;
        m_simulatedTrigger -= deltaTime;
    }
    m_aiming = combatActive && input.IsActionDown("AimDownSights");

    const int rounds = m_inventory.Update(deltaTime, held, pressed);
    const std::vector<float> blooms = m_inventory.LastShotBlooms();
    for (int i = 0; i < rounds; ++i)
    {
        FireLoadoutRound(static_cast<std::size_t>(i) < blooms.size() ? blooms[i] : m_inventory.Bloom());
    }

    const game::WeaponDef* active = m_inventory.Active();
    const std::string id = active ? active->id : std::string();
    if (m_inventory.ConsumeDryFire())
    {
        PlayClip("weapon_empty_" + id, std::string());
    }
    if (m_inventory.ConsumeReloadStarted())
    {
        PlayClip("weapon_reload_" + id, std::string());
    }
    m_inventory.ConsumeReloadFinished();

    UpdateAimZoom(deltaTime, m_aiming, active ? active->aimZoom : 1.0f);
    RefreshWeaponModels(deltaTime);
}

void Player::FireLoadoutRound(float bloom)
{
    const game::WeaponDef* weapon = m_inventory.Active();
    if (!weapon)
    {
        return;
    }

    // Spread: a random point inside the cone, using the bloom this round was
    // actually fired at (so a first tap is as accurate as the weapon allows).
    const float spread = glm::radians(m_inventory.SpreadForBloom(bloom, m_aiming));

    const glm::quat rotation = GetWorldRotation();
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);

    float rx = m_inventory.RandomSigned();
    float ry = m_inventory.RandomSigned();
    const float len2 = rx * rx + ry * ry;
    if (len2 > 1.0f)
    {
        const float len = std::sqrt(len2);
        rx /= len;
        ry /= len;
    }
    const glm::vec3 direction = glm::normalize(forward + std::tan(spread) * (rx * right + ry * up));

    Shot shot;
    shot.origin = GetWorldPosition();
    shot.direction = direction;
    shot.damage = weapon->damage;
    m_shots.push_back(shot);

    // Recoil after the round leaves, so it affects the *next* shot, not this one.
    const float kick = m_inventory.RecoilKickDegrees();
    if (m_playerControllerComponent && kick > 0.0f)
    {
        m_playerControllerComponent->AddLookOffset(kick, m_inventory.RandomSigned() * kick * 0.25f);
    }
    m_modelKick = std::min(1.0f, m_modelKick + 0.6f);

    PlayClip("weapon_fire_" + weapon->id, m_config.weapon.audioName);

    // Visual streak from the active model's muzzle.
    if (m_tracerMesh && m_tracerMaterial && m_scene)
    {
        glm::vec3 muzzle = shot.origin + forward * 0.6f;
        const std::size_t index = m_inventory.ActiveIndex();
        if (index < m_weaponModels.size() && m_weaponModels[index])
        {
            if (auto* marker = m_weaponModels[index]->FindChildByName("Muzzle"))
            {
                muzzle = marker->GetWorldPosition();
            }
        }
        auto* tracer = m_scene->CreateObject<game::Tracer>("Tracer");
        tracer->AddComponent(new eng::MeshComponent(m_tracerMaterial, m_tracerMesh));
        tracer->Launch(muzzle, direction, kTracerSpeed, kTracerLifetime);
    }
}

void Player::RefreshWeaponModels(float deltaTime)
{
    m_modelKick = std::max(0.0f, m_modelKick - kModelKickRecovery * deltaTime);
    const std::size_t active = m_inventory.ActiveIndex();
    for (std::size_t i = 0; i < m_weaponModels.size(); ++i)
    {
        eng::GameObject* model = m_weaponModels[i];
        if (!model)
        {
            continue;
        }
        const bool show = i == active;
        model->SetActive(show);
        if (!show)
        {
            continue;
        }
        // Animation-less fallback: dip on draw, dip mid-reload, kick back on fire.
        const float drawOffset = -kDrawDrop * (1.0f - m_inventory.DrawProgress());
        const float reloadOffset = m_inventory.IsReloading()
            ? -kReloadDip * std::sin(3.14159265f * m_inventory.ReloadProgress()) : 0.0f;
        glm::vec3 position = m_weaponModelRest[i];
        position.y += drawOffset + reloadOffset;
        position.z += kModelKickDistance * m_modelKick;
        model->SetPosition(position);
    }
}

void Player::PlayClip(const std::string& preferred, const std::string& fallback)
{
    if (!m_audioComponent)
    {
        return;
    }
    if (!preferred.empty() && m_audioComponent->HasClip(preferred))
    {
        m_audioComponent->Play(preferred);
    }
    else if (!fallback.empty())
    {
        m_audioComponent->Play(fallback);
    }
}

void Player::UpdateAimZoom(float deltaTime, bool aiming, float zoomFactor)
{
    if (!m_cameraComponent)
    {
        return;
    }
    const bool zooming = aiming && zoomFactor > 1.0001f;
    const float target = zooming ? 1.0f : 0.0f;
    if (target == 0.0f && m_zoomBlend == 0.0f)
    {
        return; // nothing to do - leave the FOV exactly as authored
    }
    // Exponential ease, ~90ms time constant - frame-rate independent and
    // snappy enough not to feel like added input lag.
    const float rate = 1.0f - std::exp(-deltaTime / 0.09f);
    m_zoomBlend += (target - m_zoomBlend) * rate;
    if (std::fabs(target - m_zoomBlend) < 0.001f)
    {
        m_zoomBlend = target;
    }
    // Ease back out with the factor we zoomed in with, even if the weapon that
    // had it was just switched away.
    if (zooming)
    {
        m_zoomFactorInUse = zoomFactor;
    }
    const float factor = std::max(1.0f, m_zoomFactorInUse);
    m_cameraComponent->SetFov(m_baseFov / (1.0f + (factor - 1.0f) * m_zoomBlend));
}

bool Player::SelectWeapon(std::size_t index)
{
    return m_useInventory && m_inventory.Select(index);
}

void Player::SetControlEnabled(bool enabled)
{
    m_controlEnabled = enabled;
    ApplyControlFlags();
}

void Player::SetCombatEnabled(bool enabled)
{
    m_combatEnabled = enabled;
    ApplyControlFlags();
}

void Player::ApplyControlFlags()
{
    if (!m_playerControllerComponent)
    {
        return;
    }
    m_playerControllerComponent->SetLookEnabled(m_controlEnabled);
    m_playerControllerComponent->SetMovementEnabled(m_controlEnabled && m_combatEnabled);
}

void Player::Teleport(const glm::vec3& worldPosition)
{
    if (m_playerControllerComponent)
    {
        m_playerControllerComponent->Teleport(worldPosition);
    }
    else
    {
        SetPosition(worldPosition);
    }
}

bool Player::ConsumeShot(Shot& out)
{
    if (m_shots.empty())
    {
        return false;
    }
    out = m_shots.front();
    m_shots.pop_front();
    return true;
}

bool Player::ConsumeShotFired(glm::vec3& outOrigin, glm::vec3& outDirection)
{
    Shot shot;
    if (!ConsumeShot(shot))
    {
        return false;
    }
    outOrigin = shot.origin;
    outDirection = shot.direction;
    return true;
}

void Player::InjectShot(const glm::vec3& origin, const glm::vec3& direction)
{
    Shot shot;
    shot.origin = origin;
    shot.direction = direction;
    if (m_useInventory && m_inventory.Active())
    {
        shot.damage = m_inventory.Active()->damage;
    }
    m_shots.push_back(shot);
}

void Player::FireProjectile()
{
    const auto& weapon = m_config.weapon;

    if (m_animationComponent)
    {
        m_animationComponent->Play(weapon.animationName, false);
    }

    if (m_audioComponent)
    {
        if (m_audioComponent->IsPlaying(weapon.audioName))
        {
            m_audioComponent->Stop(weapon.audioName);
        }
        m_audioComponent->Play(weapon.audioName);
    }

    auto material = eng::Material::Load(weapon.projectileMaterial);
    if (!material)
    {
        std::cerr << "Failed to load projectile material '" << weapon.projectileMaterial << "'." << std::endl;
        return;
    }

    auto bullet = m_scene->CreateObject<Bullet>("Bullet");
    bullet->SetLifetime(weapon.projectileLifetime);

    auto mesh = eng::Mesh::CreateSphere(
        weapon.projectileRadius, weapon.projectileSectors, weapon.projectileStacks);
    bullet->AddComponent(new eng::MeshComponent(material, mesh));

    glm::vec3 spawnPosition = GetWorldPosition();
    if (auto muzzle = FindChildByName(weapon.muzzleObjectName))
    {
        spawnPosition = muzzle->GetWorldPosition();
    }
    bullet->SetPosition(spawnPosition + m_rotation * weapon.projectileSpawnOffset);

    auto collider = std::make_shared<eng::SphereCollider>(weapon.projectileRadius);
    auto rigidBody = std::make_shared<eng::RigidBody>(
        eng::BodyType::Dynamic, collider, weapon.projectileMass, weapon.projectileFriction);
    bullet->AddComponent(new eng::PhysicsComponent(rigidBody));

    const glm::vec3 front = m_rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    rigidBody->ApplyImpulse(front * weapon.projectileImpulse);

    // Authoritative hit registration is a hitscan from the camera; the projectile
    // above is the visible tracer / physics prop. Record the ray for the
    // ShootingController to resolve this frame.
    Shot shot;
    shot.origin = GetWorldPosition();
    shot.direction = GetWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
    m_shots.push_back(shot);
}
