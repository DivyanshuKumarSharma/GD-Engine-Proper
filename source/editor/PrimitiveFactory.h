#pragma once

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace editor
{
    // Builds scene-schema object documents for the editor's "Add" menu. Nothing
    // here touches the live scene - the caller wraps the doc in a
    // CreateObjectCommand so every add is undoable.
    namespace PrimitiveFactory
    {
        struct Entry
        {
            std::string key;      // stable id used by menus/tests
            std::string label;    // shown to the user
            std::string group;    // "Targets" | "Shapes" | "Environment"
        };

        const std::vector<Entry>& Catalog();

        // `key` is one of Catalog()[i].key. `position` is where to drop it.
        nlohmann::json Make(const std::string& key, const glm::vec3& position);

        // Circle / square / triangle shootable targets (acceptance item 3).
        nlohmann::json MakeTarget(const std::string& shape, const glm::vec3& position,
                                  const glm::vec3& colour = glm::vec3(0.90f, 0.20f, 0.22f),
                                  float size = 1.2f);

        // A textured quad target from an imported image (project-relative path).
        // `aspect` = width / height of the source image; height is normalised to
        // `heightMetres` so the picture is never stretched.
        nlohmann::json MakeImageTarget(const std::string& imageRelPath, float aspect,
                                       const glm::vec3& position, float heightMetres = 1.4f);

        // A glTF model target with an explicit, editable box hit collider.
        nlohmann::json MakeModelTarget(const std::string& modelRelPath, const glm::vec3& position,
                                       const glm::vec3& colliderSize = glm::vec3(1.0f));
    }
}
