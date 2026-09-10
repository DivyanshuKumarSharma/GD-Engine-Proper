#include "objectives/ObjectiveFactory.h"

#include "objectives/FinishDrillObjective.h"
#include "objectives/KnockObjective.h"
#include "objectives/ReachZoneObjective.h"
#include "objectives/ShootObjective.h"
#include "util/Log.h"

namespace game
{
    std::unique_ptr<Objective> ObjectiveFactory::Create(const ObjectiveDefinition& definition)
    {
        const std::string& type = definition.type;

        if (type == "shoot")
        {
            return std::make_unique<ShootObjective>(
                definition.description, definition.amount,
                TargetKindFromString(definition.targetKind), definition.tag);
        }
        if (type == "knock")
        {
            return std::make_unique<KnockObjective>(
                definition.description, definition.amount, definition.tag);
        }
        if (type == "reach")
        {
            return std::make_unique<ReachZoneObjective>(definition.description, definition.tag);
        }
        if (type == "finish")
        {
            return std::make_unique<FinishDrillObjective>(
                definition.description.empty() ? "Complete the Drill" : definition.description);
        }

        log::Warn("Objective", "unknown objective type '" + type + "'");
        return nullptr;
    }
}
