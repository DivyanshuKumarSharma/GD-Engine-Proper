#pragma once

#include "objectives/Objective.h"

namespace game
{
    // "Knock N objects off their platforms / into a marked area". Counts
    // ObjectKnocked events, optionally filtered by tag.
    class KnockObjective : public Objective
    {
    public:
        KnockObjective(std::string description, int amount, std::string tagFilter = {});

    protected:
        bool Accepts(const GameEvent& event) const override;

    private:
        std::string m_tagFilter;
    };
}
