#pragma once

#include <glm/vec3.hpp>

#include <string>

namespace game
{
    // What kind of shootable / interactive thing an event came from. Drives
    // objective filtering ("Shoot 2 Red Targets", "Hit 3 Moving Targets").
    enum class TargetKind
    {
        Generic,
        Cube,
        RedTarget,
        MovingTarget
    };

    const char* ToString(TargetKind kind);
    TargetKind TargetKindFromString(const std::string& value);

    enum class GameEventType
    {
        TargetShot,     // a Target was hit by the weapon
        ObjectKnocked,  // a KnockTarget met its trigger condition (fell / entered area)
        ZoneReached     // the player entered a ReachZone
    };

    // A single gameplay fact the DrillRunner feeds to every active Objective.
    // Objectives inspect it and decide whether it advances their progress.
    struct GameEvent
    {
        GameEventType type = GameEventType::TargetShot;
        TargetKind targetKind = TargetKind::Generic;
        std::string tag;                    // free-form group id from the drill file
        glm::vec3 position = glm::vec3(0.0f);
    };
}
