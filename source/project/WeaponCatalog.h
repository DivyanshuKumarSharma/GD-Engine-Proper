#pragma once

#include "project/CourseProject.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace game
{
    // Small helpers shared by the editor and the runtime so weapon behaviour is
    // driven entirely by WeaponDef data, never by hard-coded scene object names.
    namespace WeaponCatalog
    {
        // The four product categories, in display order.
        const std::vector<std::string>& Categories();
        std::string CategoryLabel(const std::string& category);
        std::string FireModeLabel(const std::string& fireMode);

        // Is this a valid category / fire mode token?
        bool IsCategory(const std::string& category);
        bool IsFireMode(const std::string& fireMode);

        // A simple original first-person blockout model for a weapon that has no
        // imported mesh: a scene-schema child object (box body + barrel) sized by
        // category. Used as the working fallback so a weapon always shows and
        // fires even before art is imported.
        nlohmann::json BlockoutModel(const WeaponDef& weapon, const std::string& childName = "WeaponModel");
    }
}
