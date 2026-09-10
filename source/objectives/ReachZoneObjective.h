#pragma once

#include "objectives/Objective.h"

namespace game
{
    // "Reach the finish zone". Completes on the first ZoneReached event whose tag
    // matches (a zone tag is required). Always a single-step objective.
    class ReachZoneObjective : public Objective
    {
    public:
        ReachZoneObjective(std::string description, std::string zoneTag);

        bool ShowsCounter() const override { return false; }

    protected:
        bool Accepts(const GameEvent& event) const override;

    private:
        std::string m_zoneTag;
    };
}
