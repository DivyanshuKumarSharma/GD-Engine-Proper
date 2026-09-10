#include "objectives/Objective.h"

#include <algorithm>
#include <utility>

namespace game
{
    Objective::Objective(ObjectiveKind kind, std::string description, int targetAmount)
        : m_kind(kind)
        , m_description(std::move(description))
        , m_targetAmount(std::max(1, targetAmount))
    {
    }

    bool Objective::Handle(const GameEvent& event)
    {
        if (IsComplete() || !Accepts(event))
        {
            return false;
        }

        m_currentAmount = std::min(m_currentAmount + 1, m_targetAmount);
        return true;
    }

    void Objective::ForceComplete()
    {
        m_currentAmount = m_targetAmount;
    }
}
