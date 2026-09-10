#pragma once

#include <string>

namespace game
{
    // A snapshot captured when a drill finishes; the run's RunSummary and the
    // DrillComplete card are built from these.
    struct DrillResult
    {
        std::string drillId;
        std::string drillName;
        double timeSeconds = 0.0;
        int shotsFired = 0;
        int hits = 0;
        int objectivesCompleted = 0;
        int totalObjectives = 0;
        int score = 0;

        int AccuracyPercent() const
        {
            return shotsFired > 0
                ? static_cast<int>((static_cast<float>(hits) / shotsFired) * 100.0f + 0.5f)
                : 0;
        }
    };
}
