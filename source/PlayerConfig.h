#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <vector>

struct WeaponConfig
{
    std::string fireAction = "Fire";
    std::string animationName = "shoot";
    std::string audioName = "shoot";
    std::string weaponObjectName = "Gun";
    std::string muzzleObjectName = "BOOM_35";
    std::vector<std::string> hiddenObjectNames = { "bullet_33", "BOOM_35" };

    float fireInterval = 0.15f;
    bool semiAuto = true; // one shot per click (range default); false = full-auto while held
    std::string projectileMaterial = "materials/suzanne.mat";
    float projectileRadius = 0.2f;
    int projectileSectors = 32;
    int projectileStacks = 32;
    float projectileMass = 10.0f;
    float projectileFriction = 0.1f;
    float projectileImpulse = 500.0f;
    float projectileLifetime = 2.0f;
    glm::vec3 projectileSpawnOffset = glm::vec3(-0.2f, 0.2f, -1.75f);
};

struct PlayerConfig
{
    std::string jumpAction = "Jump";
    std::string moveForwardAction = "MoveForward";
    std::string moveBackwardAction = "MoveBackward";
    std::string moveLeftAction = "MoveLeft";
    std::string moveRightAction = "MoveRight";
    std::string jumpAudioName = "jump";
    std::string footstepsAudioName = "step";
    WeaponConfig weapon;

    static bool Load(const std::string& assetPath, PlayerConfig& result, std::string* errorMessage = nullptr);
};
