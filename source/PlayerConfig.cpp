#include "PlayerConfig.h"

#include <eng.h>
#include <nlohmann/json.hpp>

#include <exception>
#include <stdexcept>
#include <utility>

namespace
{
    void RequirePositive(float value, const std::string& property)
    {
        if (value <= 0.0f)
        {
            throw std::runtime_error("Player config property '" + property + "' must be greater than zero.");
        }
    }
}

void WeaponConfig::ApplyOverrides(const nlohmann::json& weapon)
{
    fireAction = weapon.value("fireAction", fireAction);
    fireInterval = weapon.value("fireInterval", fireInterval);
    semiAuto = weapon.value("semiAuto", semiAuto);
    animationName = weapon.value("animation", animationName);
    audioName = weapon.value("audio", audioName);
    weaponObjectName = weapon.value("weaponObject", weaponObjectName);
    muzzleObjectName = weapon.value("muzzleObject", muzzleObjectName);
    aimZoomFactor = weapon.value("aimZoomFactor", aimZoomFactor);

    if (weapon.contains("hiddenObjects"))
    {
        hiddenObjectNames = weapon["hiddenObjects"].get<std::vector<std::string>>();
    }

    if (weapon.contains("projectile"))
    {
        const auto& projectile = weapon["projectile"];
        projectileMaterial = projectile.value("material", projectileMaterial);
        projectileRadius = projectile.value("radius", projectileRadius);
        projectileSectors = projectile.value("sectors", projectileSectors);
        projectileStacks = projectile.value("stacks", projectileStacks);
        projectileMass = projectile.value("mass", projectileMass);
        projectileFriction = projectile.value("friction", projectileFriction);
        projectileImpulse = projectile.value("impulse", projectileImpulse);
        projectileLifetime = projectile.value("lifetime", projectileLifetime);

        if (projectile.contains("spawnOffset"))
        {
            const auto& offset = projectile["spawnOffset"];
            projectileSpawnOffset = glm::vec3(
                offset.value("x", projectileSpawnOffset.x),
                offset.value("y", projectileSpawnOffset.y),
                offset.value("z", projectileSpawnOffset.z));
        }
    }
}

bool PlayerConfig::Load(const std::string& assetPath, PlayerConfig& result, std::string* errorMessage)
{
    try
    {
        const std::string contents = eng::Engine::GetInstance().GetFileSystem().LoadAssetFileText(assetPath);
        if (contents.empty())
        {
            throw std::runtime_error("Could not load player config asset '" + assetPath + "'.");
        }

        const auto root = nlohmann::json::parse(contents);
        PlayerConfig loaded;

        if (root.contains("actions"))
        {
            const auto& actions = root["actions"];
            loaded.jumpAction = actions.value("jump", loaded.jumpAction);
            loaded.moveForwardAction = actions.value("forward", loaded.moveForwardAction);
            loaded.moveBackwardAction = actions.value("backward", loaded.moveBackwardAction);
            loaded.moveLeftAction = actions.value("left", loaded.moveLeftAction);
            loaded.moveRightAction = actions.value("right", loaded.moveRightAction);
        }

        if (root.contains("audio"))
        {
            const auto& audio = root["audio"];
            loaded.jumpAudioName = audio.value("jump", loaded.jumpAudioName);
            loaded.footstepsAudioName = audio.value("footsteps", loaded.footstepsAudioName);
        }

        if (root.contains("weapon"))
        {
            loaded.weapon.ApplyOverrides(root["weapon"]);
        }

        RequirePositive(loaded.weapon.fireInterval, "weapon.fireInterval");
        RequirePositive(loaded.weapon.projectileRadius, "weapon.projectile.radius");
        RequirePositive(loaded.weapon.projectileMass, "weapon.projectile.mass");
        RequirePositive(loaded.weapon.projectileImpulse, "weapon.projectile.impulse");
        RequirePositive(loaded.weapon.projectileLifetime, "weapon.projectile.lifetime");
        if (loaded.weapon.projectileSectors < 3 || loaded.weapon.projectileStacks < 2)
        {
            throw std::runtime_error("Projectile sphere requires at least 3 sectors and 2 stacks.");
        }

        result = std::move(loaded);
        return true;
    }
    catch (const std::exception& exception)
    {
        if (errorMessage)
        {
            *errorMessage = exception.what();
        }
        return false;
    }
}
