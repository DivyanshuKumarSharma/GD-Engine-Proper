#include "Player.h"
#include "Bullet.h"

#include <algorithm>
#include <cmath>
#include <iostream>

void Player::LoadProperties(const nlohmann::json& json)
{
    m_configPath = json.value("config", m_configPath);
    // A per-play weapon override baked into the scene JSON (the editor uses this
    // to equip whichever WeaponDef a drill allows without touching the project's
    // config/player.json). Absent for normally-authored scenes.
    if (json.contains("weaponOverride") && json["weaponOverride"].is_object())
    {
        m_weaponOverride = json["weaponOverride"];
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

    ApplyControlFlags();
}

void Player::Update(float deltaTime)
{
    eng::GameObject::Update(deltaTime);
    m_fireCooldown = std::max(0.0f, m_fireCooldown - deltaTime);

    auto& input = eng::Engine::GetInstance().GetInputManager();
    const bool combatActive = m_controlEnabled && m_combatEnabled;

    const bool wantsToFire = m_config.weapon.semiAuto
        ? input.WasActionPressed(m_config.weapon.fireAction)
        : input.IsActionDown(m_config.weapon.fireAction);

    if (combatActive && wantsToFire && m_fireCooldown <= 0.0f)
    {
        FireProjectile();
        m_fireCooldown = m_config.weapon.fireInterval;
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

    // Aim-down-sights zoom: hold to narrow the FOV toward 1/aimZoomFactor of the
    // base value. No-op (zoom factor 1) for weapons that don't define one.
    if (m_cameraComponent && m_config.weapon.aimZoomFactor > 1.0001f)
    {
        const bool aiming = combatActive && input.IsActionDown("AimDownSights");
        const float target = aiming ? 1.0f : 0.0f;
        // Exponential ease, ~90ms time constant - frame-rate independent and
        // snappy enough not to feel like added input lag.
        const float rate = 1.0f - std::exp(-deltaTime / 0.09f);
        m_zoomBlend += (target - m_zoomBlend) * rate;
        if (std::fabs(target - m_zoomBlend) < 0.001f)
        {
            m_zoomBlend = target;
        }
        const float fov = m_baseFov / (1.0f + (m_config.weapon.aimZoomFactor - 1.0f) * m_zoomBlend);
        m_cameraComponent->SetFov(fov);
    }
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

bool Player::ConsumeShotFired(glm::vec3& outOrigin, glm::vec3& outDirection)
{
    if (!m_shotPending)
    {
        return false;
    }
    m_shotPending = false;
    outOrigin = m_shotOrigin;
    outDirection = m_shotDirection;
    return true;
}

void Player::InjectShot(const glm::vec3& origin, const glm::vec3& direction)
{
    m_shotPending = true;
    m_shotOrigin = origin;
    m_shotDirection = direction;
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
    m_shotPending = true;
    m_shotOrigin = GetWorldPosition();
    m_shotDirection = GetWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
}
