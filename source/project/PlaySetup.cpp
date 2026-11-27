#include "project/PlaySetup.h"

#include "project/WeaponCatalog.h"

#include <algorithm>

namespace game
{
    namespace PlaySetup
    {
        nlohmann::json* FindPlayer(nlohmann::json& node)
        {
            if (node.is_object())
            {
                if (node.value("type", "") == "Player" || node.value("name", "") == "MainPlayer")
                {
                    return &node;
                }
                for (auto& [key, value] : node.items())
                {
                    (void)key;
                    if (value.is_structured())
                    {
                        if (auto* found = FindPlayer(value)) return found;
                    }
                }
            }
            else if (node.is_array())
            {
                for (auto& item : node)
                {
                    if (auto* found = FindPlayer(item)) return found;
                }
            }
            return nullptr;
        }

        void ApplyMovement(nlohmann::json& sceneDoc, const MovementSettings& mv)
        {
            nlohmann::json* player = FindPlayer(sceneDoc);
            if (!player || !player->contains("components") || !(*player)["components"].is_array())
            {
                return;
            }
            for (auto& c : (*player)["components"])
            {
                if (c.value("type", "") != "PlayerControllerComponent") continue;
                c["moveModel"] = "accel";
                c["moveSpeed"] = mv.moveSpeed;
                c["jumpHeight"] = mv.jumpHeight;
                c["groundAccel"] = mv.groundAccel;
                c["groundDecel"] = mv.groundDecel;
                c["friction"] = mv.friction;
                c["airControl"] = mv.airControl;
                c["lookSensitivity"] = mv.mouseSensitivity * 6.0f; // map to engine units
                return;
            }
        }

        void ApplyLoadout(nlohmann::json& sceneDoc, const std::vector<WeaponDef>& weapons)
        {
            nlohmann::json* player = FindPlayer(sceneDoc);
            if (!player || weapons.empty())
            {
                return;
            }

            if (player->contains("components") && (*player)["components"].is_array())
            {
                for (auto& c : (*player)["components"])
                {
                    if (c.value("type", "") != "AudioComponent") continue;
                    if (!c.contains("audio") || !c["audio"].is_array()) c["audio"] = nlohmann::json::array();
                    auto& clips = c["audio"];
                    for (auto& clip : clips)
                    {
                        if (clip.value("name", "") == "shoot") clip["voices"] = 4; // default fire sound overlaps too
                    }
                    const auto add = [&clips](const std::string& name, const std::string& path, int voices)
                    {
                        if (path.empty()) return;
                        clips.push_back({ { "name", name }, { "path", path }, { "voices", voices } });
                    };
                    for (const auto& w : weapons)
                    {
                        add("weapon_fire_" + w.id, w.fireSound, 4);
                        add("weapon_reload_" + w.id, w.reloadSound, 1);
                        add("weapon_empty_" + w.id, w.emptySound, 1);
                    }
                    break;
                }
            }

            nlohmann::json loadout = nlohmann::json::array();
            for (const auto& w : weapons)
            {
                loadout.push_back(w.ToJson());
            }
            (*player)["loadout"] = std::move(loadout);
            player->erase("weaponOverride");

            if (!player->contains("children") || !(*player)["children"].is_array())
            {
                (*player)["children"] = nlohmann::json::array();
            }
            auto& kids = (*player)["children"];
            kids.erase(std::remove_if(kids.begin(), kids.end(), [](const nlohmann::json& c)
            {
                const std::string name = c.value("name", "");
                return name == "Gun" || name.rfind("Weapon_", 0) == 0;
            }), kids.end());
            for (std::size_t i = 0; i < weapons.size(); ++i)
            {
                kids.push_back(WeaponCatalog::EquipModel(weapons[i], "Weapon_" + std::to_string(i)));
            }
        }

        void StripDrillRoot(nlohmann::json& sceneDoc)
        {
            if (!sceneDoc.contains("objects") || !sceneDoc["objects"].is_array())
            {
                return;
            }
            auto& objects = sceneDoc["objects"];
            objects.erase(std::remove_if(objects.begin(), objects.end(),
                [](const nlohmann::json& o) { return o.value("name", "") == "DrillRoot"; }), objects.end());
        }

        std::vector<WeaponDef> ResolveLoadout(const CourseProject& project, const DrillDoc* drill)
        {
            std::vector<WeaponDef> loadout;
            if (drill)
            {
                for (const auto& id : drill->allowedWeapons)
                {
                    if (const auto* weapon = project.FindWeapon(id))
                    {
                        loadout.push_back(*weapon);
                    }
                }
            }
            if (loadout.empty())
            {
                loadout = project.weapons;
            }
            if (loadout.size() > 9)
            {
                loadout.resize(9);
            }
            return loadout;
        }

        nlohmann::json MergeInputMap(const nlohmann::json& baseMap, const KeyBindings& bindings,
                                     std::vector<std::string>* unsupported)
        {
            nlohmann::json map = baseMap.is_object() ? baseMap : nlohmann::json::object();
            if (!map.contains("actions") || !map["actions"].is_object())
            {
                map["actions"] = nlohmann::json::object();
            }
            const nlohmann::json bound = bindings.ToInputActions(unsupported);
            for (auto it = bound.begin(); it != bound.end(); ++it)
            {
                map["actions"][it.key()] = it.value();
            }
            return map;
        }
    }
}
