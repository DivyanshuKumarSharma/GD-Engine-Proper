#pragma once

#include "drills/DrillDefinition.h"

#include <string>
#include <vector>

namespace game
{
    // Loads drill definitions from assets. The course manifest (drills/course.json)
    // is just an ordered list of drill files:
    //   { "drills": ["drills/drill01.json", "drills/drill02.json", "drills/drill03.json"] }
    class DrillCatalog
    {
    public:
        // Returns the drills in play order. Empty (with a logged warning) if the
        // manifest or any drill file is missing or malformed.
        static std::vector<DrillDefinition> Load(const std::string& courseAsset = "drills/course.json");

        static bool LoadDrill(const std::string& assetPath, DrillDefinition& out, std::string* error);
    };
}
