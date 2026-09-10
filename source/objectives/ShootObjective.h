#pragma once

#include "objectives/Objective.h"

namespace game
{
    // "Shoot N targets", optionally restricted to a target kind and/or a tag.
    // Covers the spec's ShootTarget, ShootTargetType and HitMovingTarget with one
    // class: kind filter Generic = any, empty tag = any.
    class ShootObjective : public Objective
    {
    public:
        ShootObjective(std::string description, int amount,
            TargetKind kindFilter = TargetKind::Generic, std::string tagFilter = {});

    protected:
        bool Accepts(const GameEvent& event) const override;

    private:
        TargetKind m_kindFilter;
        std::string m_tagFilter;
    };
}
