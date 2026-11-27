#pragma once

#include "project/CourseProject.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace game
{
    // Turns an authored scene document + CourseProject settings into the scene
    // that actually gets played. Shared by the editor's playtest and the
    // standalone course player, so both run the exact same setup.
    namespace PlaySetup
    {
        // The MainPlayer object's doc anywhere in a scene-schema tree.
        nlohmann::json* FindPlayer(nlohmann::json& sceneDoc);

        // Bake movement settings into the player's controller (accel model,
        // jump height in metres, sensitivity).
        void ApplyMovement(nlohmann::json& sceneDoc, const MovementSettings& movement);

        // Give the player a loadout: "Weapon_<i>" model children, the WeaponDefs
        // under "loadout", and the weapons' sounds as named clips. Replaces any
        // legacy "Gun" child. No-op for an empty list.
        void ApplyLoadout(nlohmann::json& sceneDoc, const std::vector<WeaponDef>& weapons);

        // Drop the editor's live drill-content layer (DrillRunner spawns drill
        // content itself during play).
        void StripDrillRoot(nlohmann::json& sceneDoc);

        // A drill's weapons: its Allowed Weapons in order, or every project
        // weapon when none are listed. At most 9 (slots 1-9).
        std::vector<WeaponDef> ResolveLoadout(const CourseProject& project, const DrillDoc* drill);

        // config/input.json's text with the project's key bindings merged over
        // the same-named actions. Unrecognised binding tokens are reported.
        nlohmann::json MergeInputMap(const nlohmann::json& baseMap, const KeyBindings& bindings,
                                     std::vector<std::string>* unsupported = nullptr);
    }
}
