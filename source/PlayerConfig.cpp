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
            const auto& weapon = root["weapon"];
            loaded.weapon.fireAction = weapon.value("fireAction", loaded.weapon.fireAction);
            loaded.weapon.fireInterval = weapon.value("fireInterval", loaded.weapon.fireInterval);
            loaded.weapon.semiAuto = weapon.value("semiAuto", loaded.weapon.semiAuto);
            loaded.weapon.animationName = weapon.value("animation", loaded.weapon.animationName);
            loaded.weapon.audioName = weapon.value("audio", loaded.weapon.audioName);
            loaded.weapon.weaponObjectName = weapon.value("weaponObject", loaded.weapon.weaponObjectName);
            loaded.weapon.muzzleObjectName = weapon.value("muzzleObject", loaded.weapon.muzzleObjectName);

            if (weapon.contains("hiddenObjects"))
            {
                loaded.weapon.hiddenObjectNames = weapon["hiddenObjects"].get<std::vector<std::string>>();
            }

            if (weapon.contains("projectile"))
            {
                const auto& projectile = weapon["projectile"];
                loaded.weapon.projectileMaterial = projectile.value("material", loaded.weapon.projectileMaterial);
                loaded.weapon.projectileRadius = projectile.value("radius", loaded.weapon.projectileRadius);
                loaded.weapon.projectileSectors = projectile.value("sectors", loaded.weapon.projectileSectors);
                loaded.weapon.projectileStacks = projectile.value("stacks", loaded.weapon.projectileStacks);
                loaded.weapon.projectileMass = projectile.value("mass", loaded.weapon.projectileMass);
                loaded.weapon.projectileFriction = projectile.value("friction", loaded.weapon.projectileFriction);
                loaded.weapon.projectileImpulse = projectile.value("impulse", loaded.weapon.projectileImpulse);
                loaded.weapon.projectileLifetime = projectile.value("lifetime", loaded.weapon.projectileLifetime);

                if (projectile.contains("spawnOffset"))
                {
                    const auto& offset = projectile["spawnOffset"];
                    loaded.weapon.projectileSpawnOffset = glm::vec3(
                        offset.value("x", loaded.weapon.projectileSpawnOffset.x),
                        offset.value("y", loaded.weapon.projectileSpawnOffset.y),
                        offset.value("z", loaded.weapon.projectileSpawnOffset.z));
                }
            }
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
