#include "Player.h"
#include "Bullet.h"

#include <algorithm>
#include <iostream>

void Player::LoadProperties(const nlohmann::json& json)
{
    m_configPath = json.value("config", m_configPath);
}

void Player::Init()
{
    std::string configError;
    if (!PlayerConfig::Load(m_configPath, m_config, &configError))
    {
        std::cerr << "Failed to load player configuration: " << configError << std::endl;
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
