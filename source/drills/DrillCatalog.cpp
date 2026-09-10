#include "drills/DrillCatalog.h"
#include "util/Log.h"

#include <eng.h>

#include <exception>

namespace game
{
    namespace
    {
        glm::vec3 ReadVec3(const nlohmann::json& parent, const char* key, const glm::vec3& fallback)
        {
            if (!parent.contains(key))
            {
                return fallback;
            }
            const auto& value = parent[key];
            return glm::vec3(
                value.value("x", fallback.x),
                value.value("y", fallback.y),
                value.value("z", fallback.z));
        }
    }

    bool DrillCatalog::LoadDrill(const std::string& assetPath, DrillDefinition& out, std::string* error)
    {
        try
        {
            const std::string contents =
                eng::Engine::GetInstance().GetFileSystem().LoadAssetFileText(assetPath);
            if (contents.empty())
            {
                throw std::runtime_error("drill file is missing or empty: " + assetPath);
            }

            const auto json = nlohmann::json::parse(contents);

            DrillDefinition definition;
            definition.id = json.value("id", assetPath);
            definition.name = json.value("name", definition.id);
            definition.intro = json.value("intro", "");
            definition.targetTimeSeconds = json.value("targetTime", 60.0f);
            definition.playerStart = ReadVec3(json, "playerStart", glm::vec3(0.0f));

            if (json.contains("objectives") && json["objectives"].is_array())
            {
                for (const auto& entry : json["objectives"])
                {
                    ObjectiveDefinition objective;
                    objective.type = entry.value("type", "");
                    objective.description = entry.value("description", "");
                    objective.amount = entry.value("amount", 1);
                    objective.targetKind = entry.value("targetKind", "");
                    objective.tag = entry.value("tag", "");
                    definition.objectives.push_back(std::move(objective));
                }
            }

            if (definition.objectives.empty())
            {
                throw std::runtime_error("drill '" + definition.id + "' defines no objectives");
            }

            if (json.contains("objects") && json["objects"].is_array())
            {
                definition.content = json["objects"];
            }

            out = std::move(definition);
            return true;
        }
        catch (const std::exception& exception)
        {
            if (error)
            {
                *error = exception.what();
            }
            return false;
        }
    }

    std::vector<DrillDefinition> DrillCatalog::Load(const std::string& courseAsset)
    {
        std::vector<DrillDefinition> drills;

        const std::string contents =
            eng::Engine::GetInstance().GetFileSystem().LoadAssetFileText(courseAsset);
        if (contents.empty())
        {
            log::Warn("Drill", "course manifest missing: " + courseAsset);
            return drills;
        }

        try
        {
            const auto json = nlohmann::json::parse(contents);
            if (!json.contains("drills") || !json["drills"].is_array())
            {
                log::Warn("Drill", "course manifest has no 'drills' array");
                return drills;
            }

            for (const auto& entry : json["drills"])
            {
                const std::string path = entry.get<std::string>();
                DrillDefinition definition;
                std::string error;
                if (DrillCatalog::LoadDrill(path, definition, &error))
                {
                    drills.push_back(std::move(definition));
                }
                else
                {
                    log::Warn("Drill", "failed to load " + path + ": " + error);
                    drills.clear();
                    return drills;
                }
            }
        }
        catch (const std::exception& exception)
        {
            log::Warn("Drill", std::string("course manifest parse error: ") + exception.what());
            drills.clear();
        }

        return drills;
    }
}
