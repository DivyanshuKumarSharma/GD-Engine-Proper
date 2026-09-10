#pragma once

#include "course/GameEvent.h"

#include <string>

namespace game
{
    enum class ObjectiveKind
    {
        Shoot,
        Knock,
        ReachZone,
        FinishDrill
    };

    // One tracked goal inside a drill: a description, a target count, and current
    // progress. Concrete subclasses only decide which GameEvents count towards
    // them (Accepts); progress accounting and completion live in the base so
    // "counts once" / clamping / done-checks are impossible to get wrong.
    class Objective
    {
    public:
        Objective(ObjectiveKind kind, std::string description, int targetAmount);
        virtual ~Objective() = default;

        // Feed a gameplay fact to this objective. Returns true only when this
        // event advanced progress (so callers can trigger a completion cue).
        bool Handle(const GameEvent& event);

        // Marked complete directly (used by FinishDrillObjective).
        void ForceComplete();

        ObjectiveKind Kind() const { return m_kind; }
        const std::string& Description() const { return m_description; }
        int TargetAmount() const { return m_targetAmount; }
        int CurrentAmount() const { return m_currentAmount; }
        bool IsComplete() const { return m_currentAmount >= m_targetAmount; }

        // Does the HUD show an [x/y] counter, or just a checkbox?
        virtual bool ShowsCounter() const { return true; }

    protected:
        virtual bool Accepts(const GameEvent& event) const = 0;

        ObjectiveKind m_kind;
        std::string m_description;
        int m_targetAmount = 1;
        int m_currentAmount = 0;
    };
}
