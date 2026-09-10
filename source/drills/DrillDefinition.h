#pragma once

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace game
{
    // One objective as authored in a drill JSON file.
    struct ObjectiveDefinition
    {
        std::string type;         // "shoot" | "knock" | "reach" | "finish"
        std::string description;  // HUD text, e.g. "Shoot Red Targets"
        int amount = 1;
        std::string targetKind;   // "cube" | "red" | "moving"  (shoot objectives)
        std::string tag;          // event filter (shoot/knock) or zone id (reach)
    };

    // A complete, data-driven drill. Loaded from assets/drills/<id>.json by
    // DrillCatalog; consumed by DrillRunner (which spawns `content` under a
    // throw-away DrillRoot and builds Objective instances from `objectives`).
    struct DrillDefinition
    {
        std::string id;                       // "drill01"
        std::string name;                     // "DRILL 01 - TARGET PRACTICE"
        std::string intro;                    // one line shown on the title card
        float targetTimeSeconds = 60.0f;      // par time, used for the time bonus
        glm::vec3 playerStart = glm::vec3(0.0f);
        std::vector<ObjectiveDefinition> objectives;
        nlohmann::json content = nlohmann::json::array(); // scene-schema object list
    };
}
