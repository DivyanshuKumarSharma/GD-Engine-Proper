#pragma once

#include "drills/DrillDefinition.h"
#include "objectives/Objective.h"

#include <memory>

namespace game
{
    // Turns an authored ObjectiveDefinition into a runtime Objective. Add a new
    // objective type here (one line) plus its class to extend the system.
    class ObjectiveFactory
    {
    public:
        static std::unique_ptr<Objective> Create(const ObjectiveDefinition& definition);
    };
}
