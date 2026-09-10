#include "stats/HighScoreStore.h"
#include "util/Log.h"

#include <eng.h>
#include <nlohmann/json.hpp>

#include <exception>
#include <utility>

namespace game
{
    HighScoreStore::HighScoreStore(std::filesystem::path filePath)
        : m_filePathOverride(std::move(filePath))
    {
    }

    std::filesystem::path HighScoreStore::ResolveFilePath() const
    {
        if (!m_filePathOverride.empty())
        {
            return m_filePathOverride;
        }
        return eng::Engine::GetInstance().GetFileSystem().GetExecutableFolder() / "highscores.json";
    }

    void HighScoreStore::Load()
    {
        m_loaded = true;
        m_scores = HighScores{};

        const std::string contents =
            eng::Engine::GetInstance().GetFileSystem().LoadTextFile(ResolveFilePath());
        if (contents.empty())
        {
            log::Info("HighScore", "no save file yet - starting fresh");
            return;
        }

        try
        {
            const auto json = nlohmann::json::parse(contents);
            m_scores.highScore = json.value("highScore", 0);
            m_scores.bestTimeSeconds = json.value("bestTime", 0.0);
            m_scores.bestAccuracy = json.value("bestAccuracy", 0.0f);
            log::Info("HighScore", "loaded (score " + std::to_string(m_scores.highScore) + ")");
        }
        catch (const std::exception& exception)
        {
            log::Warn("HighScore", std::string("save file is corrupt, ignoring: ") + exception.what());
            m_scores = HighScores{};
        }
    }

    HighScoreStore::SubmitResult HighScoreStore::Submit(
        int score, double completionTimeSeconds, float accuracy)
    {
        if (!m_loaded)
        {
            Load();
        }

        SubmitResult result;

        if (score > m_scores.highScore)
        {
            m_scores.highScore = score;
            result.newHighScore = true;
        }

        if (completionTimeSeconds > 0.0 &&
            (m_scores.bestTimeSeconds <= 0.0 || completionTimeSeconds < m_scores.bestTimeSeconds))
        {
            m_scores.bestTimeSeconds = completionTimeSeconds;
            result.newBestTime = true;
        }

        if (accuracy > m_scores.bestAccuracy)
        {
            m_scores.bestAccuracy = accuracy;
            result.newBestAccuracy = true;
        }

        if (result.Any())
        {
            if (Save())
            {
                log::Info("HighScore", "new record saved");
            }
            else
            {
                log::Warn("HighScore", "failed to write save file");
            }
        }

        return result;
    }

    bool HighScoreStore::Save() const
    {
        const nlohmann::json json = {
            { "highScore", m_scores.highScore },
            { "bestTime", m_scores.bestTimeSeconds },
            { "bestAccuracy", m_scores.bestAccuracy },
        };
        return eng::Engine::GetInstance().GetFileSystem().SaveTextFile(
            ResolveFilePath(), json.dump(2));
    }
}
