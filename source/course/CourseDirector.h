#pragma once

#include "course/DrillResult.h"
#include "course/GameState.h"
#include "drills/DrillDefinition.h"
#include "stats/HighScoreStore.h"
#include "stats/PlayerStats.h"
#include "stats/ScoreCalculator.h"
#include "util/Stopwatch.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Player;

namespace eng
{
    class Scene;
    class GameObject;
    class CanvasComponent;
}

namespace game
{
    class DrillRunner;
    class ShootingController;
    class HudView;

    // Owns the whole course lifecycle: the state machine, the run/drill timers,
    // accuracy stats, scoring and persistence. Driven by Game::Update BEFORE the
    // scene updates, so anything it spawns or resets happens while the scene is
    // not mid-iteration. Holds no rendering code - the HudView observes it through
    // the const accessors below; it never drives the director.
    class CourseDirector
    {
    public:
        CourseDirector();
        ~CourseDirector();

        void Init(eng::Scene* scene, Player* player, eng::CanvasComponent* menuCanvas);
        void Update(float deltaTime);

        // Shrink the intro / countdown / drill-complete dwells (used by the
        // headless self-test so a full course runs in seconds).
        void SetFastTransitions(bool fast);

        // Input hooks routed from Game.
        void OnPausePressed();
        void OnConfirmPressed();   // "continue" / "retry" depending on state

        // Course control (Play button, results screen, and the self-test harness).
        void StartRun();
        void RetryRun();
        void ReturnToMenu();

        // Test/automation hook: instantly satisfy the active drill's objectives.
        void DebugCompleteActiveDrill();

        // ---- Observation only (HudView + tests); never mutates state ----
        GameState State() const { return m_state; }
        const DrillDefinition* CurrentDrillDefinition() const { return CurrentDrill(); }
        std::size_t CurrentDrillIndex() const { return m_currentDrill; }
        std::size_t DrillCount() const { return m_drills.size(); }
        const DrillRunner& Runner() const;
        const PlayerStats& Stats() const { return m_stats; }
        double RunTimeSeconds() const;
        double DrillTimeSeconds() const;
        int CountdownNumber() const;   // 3..1, 0 == "GO", -1 == not counting
        const std::vector<DrillResult>& DrillResults() const { return m_drillResults; }
        const DrillResult* LastDrillResult() const;
        const ScoreBreakdown& FinalScore() const { return m_finalScore; }
        HighScoreStore::SubmitResult FinalRecords() const { return m_finalRecords; }
        const HighScores& Records() const { return m_highScores.Get(); }

        std::function<void()> onQuitRequested;

    private:
        void EnterState(GameState next);
        void UpdateState(float deltaTime);

        void BeginDrill(std::size_t index);
        void CompleteDrill();
        void CompleteRun();
        void ResetRunState();
        void RefreshHud();

        void SetWorldFrozen(bool frozen);
        const DrillDefinition* CurrentDrill() const;

        eng::Scene* m_scene = nullptr;
        Player* m_player = nullptr;
        eng::CanvasComponent* m_menuCanvas = nullptr;

        GameState m_state = GameState::MainMenu;
        std::vector<DrillDefinition> m_drills;
        std::size_t m_currentDrill = 0;

        std::unique_ptr<DrillRunner> m_runner;
        std::unique_ptr<ShootingController> m_shooting;
        std::unique_ptr<HudView> m_hud;

        PlayerStats m_stats;
        PlayerStats m_statsAtDrillStart;
        Stopwatch m_runTimer;
        Stopwatch m_drillTimer;
        HighScoreStore m_highScores;

        std::vector<DrillResult> m_drillResults;
        ScoreBreakdown m_finalScore;
        HighScoreStore::SubmitResult m_finalRecords;

        float m_introSeconds = 1.8f;
        float m_countdownSeconds = 3.0f;
        float m_drillCompleteSeconds = 3.2f;

        float m_stateTimer = 0.0f;        // intro / drill-complete dwell
        float m_countdownRemaining = 0.0f;
        int m_countdownShown = 0;
        bool m_drillCompletePending = false;
    };
}
