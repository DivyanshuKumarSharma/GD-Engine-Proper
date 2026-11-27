#include "project/WeaponCatalog.h"

#include <algorithm>

namespace game
{
    namespace WeaponCatalog
    {
        const std::vector<std::string>& Categories()
        {
            static const std::vector<std::string> categories = { "pistol", "smg", "auto", "sniper" };
            return categories;
        }

        std::string CategoryLabel(const std::string& category)
        {
            if (category == "pistol") return "Pistol";
            if (category == "smg") return "SMG";
            if (category == "auto") return "Automatic Rifle";
            if (category == "sniper") return "Sniper Rifle";
            return "Unknown";
        }

        std::string FireModeLabel(const std::string& fireMode)
        {
            if (fireMode == "semi") return "Semi-automatic";
            if (fireMode == "auto") return "Full-automatic";
            if (fireMode == "burst") return "Burst";
            return "Unknown";
        }

        bool IsCategory(const std::string& category)
        {
            const auto& c = Categories();
            return std::find(c.begin(), c.end(), category) != c.end();
        }

        bool IsFireMode(const std::string& fireMode)
        {
            return fireMode == "semi" || fireMode == "auto" || fireMode == "burst";
        }

        nlohmann::json BlockoutModel(const WeaponDef& weapon, const std::string& childName)
        {
            // Body proportions per category (metres). Deliberately simple; a real
            // imported model replaces this entirely.
            float bodyLen = 0.34f;
            float bodyH = 0.12f;
            float barrelLen = 0.16f;
            if (weapon.category == "smg") { bodyLen = 0.40f; barrelLen = 0.14f; }
            else if (weapon.category == "auto") { bodyLen = 0.52f; barrelLen = 0.26f; }
            else if (weapon.category == "sniper") { bodyLen = 0.62f; barrelLen = 0.44f; bodyH = 0.10f; }

            const auto grey = nlohmann::json::array({
                { { "name", "color" }, { "value0", 0.16 }, { "value1", 0.17 }, { "value2", 0.2 } } });
            const auto darkGrey = nlohmann::json::array({
                { { "name", "color" }, { "value0", 0.09 }, { "value1", 0.09 }, { "value2", 0.11 } } });

            nlohmann::json body = {
                { "name", "Body" },
                { "position", { { "x", 0 }, { "y", 0 }, { "z", -bodyLen * 0.5f } } },
                { "components", nlohmann::json::array({
                    { { "type", "MeshComponent" },
                      { "mesh", { { "type", "box" }, { "x", 0.045f }, { "y", bodyH * 0.85f }, { "z", bodyLen } } },
                      { "material", { { "path", "materials/checker.mat" }, { "params", { { "float3", grey } } } } } } }) },
            };
            nlohmann::json barrel = {
                { "name", "Barrel" },
                { "position", { { "x", 0 }, { "y", bodyH * 0.15f }, { "z", -bodyLen - barrelLen * 0.5f } } },
                { "components", nlohmann::json::array({
                    { { "type", "MeshComponent" },
                      { "mesh", { { "type", "box" }, { "x", 0.03f }, { "y", 0.03f }, { "z", barrelLen } } },
                      { "material", { { "path", "materials/checker.mat" }, { "params", { { "float3", darkGrey } } } } } } }) },
            };
            // No mesh - a pure transform marker at the barrel tip so Player can
            // resolve "muzzleObject" for projectile spawn / muzzle-flash position.
            nlohmann::json muzzle = {
                { "name", "Muzzle" },
                { "position", { { "x", 0 }, { "y", bodyH * 0.15f }, { "z", -bodyLen - barrelLen } } },
            };

            return {
                { "name", childName },
                { "position", { { "x", weapon.gripOffset.x }, { "y", weapon.gripOffset.y }, { "z", weapon.gripOffset.z } } },
                { "scale", { { "x", weapon.modelScale }, { "y", weapon.modelScale }, { "z", weapon.modelScale } } },
                { "children", nlohmann::json::array({ body, barrel, muzzle }) },
            };
        }

        nlohmann::json EquipModel(const WeaponDef& weapon, const std::string& childName)
        {
            if (weapon.modelPath.empty())
            {
                return BlockoutModel(weapon, childName);
            }

            // An imported mesh has no guaranteed "Muzzle" node or fire animation;
            // Player falls back to the owner's transform / no animation, which is
            // the documented, working behaviour for a mesh-only import.
            return {
                { "name", childName },
                { "type", "gltf" },
                { "path", weapon.modelPath },
                { "position", { { "x", weapon.gripOffset.x }, { "y", weapon.gripOffset.y }, { "z", weapon.gripOffset.z } } },
                { "scale", { { "x", weapon.modelScale }, { "y", weapon.modelScale }, { "z", weapon.modelScale } } },
            };
        }

        nlohmann::json PlayerOverrideDoc(const WeaponDef& weapon, const std::string& childName)
        {
            // "audio" (a registered AudioComponent clip *name*, not a file path)
            // is intentionally left unset here - the caller registers the weapon's
            // fireSound on the player's AudioComponent under a stable name (if any
            // is authored) and sets it, since that requires editing a sibling
            // component's doc that this function doesn't have access to.
            return {
                { "weaponObject", childName },
                { "muzzleObject", "Muzzle" },
                { "fireInterval", weapon.FireInterval() },
                { "semiAuto", weapon.fireMode != "auto" },
                { "aimZoomFactor", weapon.aimZoom },
                { "hiddenObjects", nlohmann::json::array() },
            };
        }
    }
}
