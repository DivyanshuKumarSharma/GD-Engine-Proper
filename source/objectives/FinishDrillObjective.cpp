#include "objectives/FinishDrillObjective.h"

#include <utility>

namespace game
{
    FinishDrillObjective::FinishDrillObjective(std::string description)
        : Objective(ObjectiveKind::FinishDrill, std::move(description), 1)
    {
    }

    bool FinishDrillObjective::Accepts(const GameEvent& /*event*/) const
    {
        return false;
    }
}
