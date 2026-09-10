#pragma once

namespace game
{
    // Everything the score depends on, gathered at the end of a run.
    struct RunSummary
    {
        int objectivesCompleted = 0;
        int targetsHit = 0;
        int shotsFired = 0;
        float accuracy = 0.0f;          // 0..1
        double completionTimeSeconds = 0.0;
        double parTimeSeconds = 0.0;    // sum of the drills' target times
    };

    struct ScoreBreakdown
    {
        int objectiveScore = 0;
        int accuracyBonus = 0;
        int timeBonus = 0;
        int total = 0;
    };

    // Deliberately simple and readable (matches the on-screen breakdown 1:1):
    //   objective score = completedObjectives * 1000
    //   accuracy bonus  = round(accuracy * 100) * 20
    //   time bonus      = max(0, par - time) * 100
    //   final           = the sum of the three
    class ScoreCalculator
    {
    public:
        static ScoreBreakdown Compute(const RunSummary& summary);

        static constexpr int kPointsPerObjective = 1000;
        static constexpr int kAccuracyBonusPerPercent = 20;
        static constexpr int kTimeBonusPerSecond = 100;
    };
}
