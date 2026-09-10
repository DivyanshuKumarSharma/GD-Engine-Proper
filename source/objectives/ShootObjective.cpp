#include "objectives/ShootObjective.h"

#include <utility>

namespace game
{
    ShootObjective::ShootObjective(std::string description, int amount,
        TargetKind kindFilter, std::string tagFilter)
        : Objective(ObjectiveKind::Shoot, std::move(description), amount)
        , m_kindFilter(kindFilter)
        , m_tagFilter(std::move(tagFilter))
    {
    }

    bool ShootObjective::Accepts(const GameEvent& event) const
    {
        if (event.type != GameEventType::TargetShot)
        {
            return false;
        }
        if (m_kindFilter != TargetKind::Generic && event.targetKind != m_kindFilter)
        {
            return false;
        }
        if (!m_tagFilter.empty() && event.tag != m_tagFilter)
        {
            return false;
        }
        return true;
    }
}
