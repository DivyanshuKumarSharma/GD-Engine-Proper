#pragma once

#include "objectives/Objective.h"

namespace game
{
    // "Complete the drill" — a flavour objective that never consumes an event.
    // The DrillRunner force-completes it once every other objective is done.
    class FinishDrillObjective : public Objective
    {
    public:
        explicit FinishDrillObjective(std::string description);

        bool ShowsCounter() const override { return false; }

    protected:
        bool Accepts(const GameEvent& event) const override;
    };
}
