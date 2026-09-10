#include "stats/ScoreCalculator.h"

#include <algorithm>

namespace game
{
    ScoreBreakdown ScoreCalculator::Compute(const RunSummary& summary)
    {
        ScoreBreakdown breakdown;

        breakdown.objectiveScore = summary.objectivesCompleted * kPointsPerObjective;

        const int accuracyPercent = static_cast<int>(summary.accuracy * 100.0f + 0.5f);
        breakdown.accuracyBonus = accuracyPercent * kAccuracyBonusPerPercent;

        const double timeSaved = summary.parTimeSeconds - summary.completionTimeSeconds;
        breakdown.timeBonus = timeSaved > 0.0
            ? static_cast<int>(timeSaved * kTimeBonusPerSecond)
            : 0;

        breakdown.total =
            breakdown.objectiveScore + breakdown.accuracyBonus + breakdown.timeBonus;

        return breakdown;
    }
}
