#include "objectives/KnockObjective.h"

#include <utility>

namespace game
{
    KnockObjective::KnockObjective(std::string description, int amount, std::string tagFilter)
        : Objective(ObjectiveKind::Knock, std::move(description), amount)
        , m_tagFilter(std::move(tagFilter))
    {
    }

    bool KnockObjective::Accepts(const GameEvent& event) const
    {
        if (event.type != GameEventType::ObjectKnocked)
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
