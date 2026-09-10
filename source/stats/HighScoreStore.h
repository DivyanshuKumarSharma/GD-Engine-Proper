#pragma once

#include <filesystem>

namespace game
{
    struct HighScores
    {
        int highScore = 0;
        double bestTimeSeconds = 0.0;  // 0 == no record yet
        float bestAccuracy = 0.0f;     // 0..1
    };

    // Persists the player's bests to "highscores.json" next to the executable
    // (never inside assets/, which is regenerated on every build). Missing or
    // corrupt files fall back to empty records; a run is never lost because a
    // save failed. A custom path can be supplied for tests.
    class HighScoreStore
    {
    public:
        struct SubmitResult
        {
            bool newHighScore = false;
            bool newBestTime = false;
            bool newBestAccuracy = false;

            bool Any() const { return newHighScore || newBestTime || newBestAccuracy; }
        };

        HighScoreStore() = default;
        explicit HighScoreStore(std::filesystem::path filePath);

        void Load();
        const HighScores& Get() const { return m_scores; }

        // Merge a finished run. Saves immediately if any record improved.
        SubmitResult Submit(int score, double completionTimeSeconds, float accuracy);

    private:
        bool Save() const;
        std::filesystem::path ResolveFilePath() const;

        std::filesystem::path m_filePathOverride;
        HighScores m_scores;
        bool m_loaded = false;
    };
}
