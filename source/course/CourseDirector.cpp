#include "course/CourseDirector.h"

#include "Player.h"
#include "drills/DrillCatalog.h"
#include "drills/DrillRunner.h"
#include "hud/HudView.h"
#include "util/Log.h"
#include "weapon/ShootingController.h"

#include <eng.h>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        void SetMenuInputActive(bool active)
        {
            auto& engine = eng::Engine::GetInstance();
            engine.SetCursorEnabled(active);
            engine.GetUIInputSystem().SetActive(active);
        }
    }

    CourseDirector::CourseDirector() = default;
    CourseDirector::~CourseDirector() = default;

    void CourseDirector::SetFastTransitions(bool fast)
    {
        const float scale = fast ? 0.06f : 1.0f;
        m_introSeconds = 1.8f * scale;
        m_countdownSeconds = 3.0f * scale;
        m_drillCompleteSeconds = 3.2f * scale;
    }

    void CourseDirector::Init(eng::Scene* scene, Player* player, eng::CanvasComponent* menuCanvas)
    {
        Init(scene, player, menuCanvas, DrillCatalog::Load());
    }

    void CourseDirector::Init(eng::Scene* scene, Player* player, eng::CanvasComponent* menuCanvas,
                              std::vector<DrillDefinition> drills)
    {
        m_scene = scene;
        m_player = player;
        m_menuCanvas = menuCanvas;

        m_drills = std::move(drills);
        if (m_drills.empty())
        {
            log::Warn("Course", "no drills loaded - the course cannot start");
        }

        m_highScores.Load();

        m_runner = std::make_unique<DrillRunner>(scene);
        m_shooting = std::make_unique<ShootingController>(player, &m_stats);
        m_hud = std::make_unique<HudView>();
        m_hud->Init(scene);

        m_shooting->onHitConfirmed.Connect([this](glm::vec3)
        {
            if (m_hud)
            {
                m_hud->FlashHitMarker();
            }
        });

        m_runner->onAllObjectivesComplete.Connect([this]()
        {
            // Defer: this fires from inside objective iteration.
            if (m_state == GameState::Playing)
            {
                m_drillCompletePending = true;
            }
        });

        m_runner->onObjectiveCompleted.Connect([this](int index)
        {
            if (const auto* definition = CurrentDrill();
                definition && index >= 0 && index < static_cast<int>(m_runner->Objectives().size()))
            {
                log::Info("Objective",
                    "[" + definition->id + "] done: " + m_runner->Objectives()[index]->Description());
            }
        });

        if (m_menuCanvas)
        {
            if (auto* owner = m_menuCanvas->GetOwner())
            {
                if (auto* play = owner->FindChildByName("PlayButton"))
                {
                    if (auto* button = play->GetComponent<eng::ButtonComponent>())
                    {
                        button->onClick = [this]() { StartRun(); };
                    }
                }
                if (auto* quit = owner->FindChildByName("QuitButton"))
                {
                    if (auto* button = quit->GetComponent<eng::ButtonComponent>())
                    {
                        button->onClick = [this]() { if (onQuitRequested) { onQuitRequested(); } };
                    }
                }
            }
        }

        EnterState(GameState::MainMenu);
    }

    void CourseDirector::Update(float deltaTime)
    {
        UpdateState(deltaTime);
        RefreshHud();
    }

    void CourseDirector::RefreshHud()
    {
        if (m_hud)
        {
            m_hud->Refresh(*this);
        }
    }

    const DrillDefinition* CourseDirector::CurrentDrill() const
    {
        return m_currentDrill < m_drills.size() ? &m_drills[m_currentDrill] : nullptr;
    }

    const DrillRunner& CourseDirector::Runner() const
    {
        return *m_runner;
    }

    double CourseDirector::RunTimeSeconds() const
    {
        return m_runTimer.ElapsedSeconds();
    }

    double CourseDirector::DrillTimeSeconds() const
    {
        return m_drillTimer.ElapsedSeconds();
    }

    int CourseDirector::CountdownNumber() const
    {
        if (m_state != GameState::Countdown)
        {
            return -1;
        }
        if (m_countdownRemaining <= 0.0f)
        {
            return 0;
        }
        return std::max(1, static_cast<int>(std::ceil(m_countdownRemaining)));
    }

    const DrillResult* CourseDirector::LastDrillResult() const
    {
        return m_drillResults.empty() ? nullptr : &m_drillResults.back();
    }

    void CourseDirector::SetWorldFrozen(bool frozen)
    {
        eng::Engine::GetInstance().GetPhysicsManager().SetPaused(frozen);
    }

    void CourseDirector::EnterState(GameState next)
    {
        m_state = next;
        log::Info("State", ToString(next));

        switch (next)
        {
        case GameState::MainMenu:
        {
            m_music.Stop();
            if (m_menuCanvas) { m_menuCanvas->SetActive(true); }
            SetMenuInputActive(true);
            if (m_player) { m_player->SetControlEnabled(false); }
            SetWorldFrozen(false);
            break;
        }
        case GameState::DrillIntro:
        {
            if (m_menuCanvas) { m_menuCanvas->SetActive(false); }
            SetMenuInputActive(false);
            SetWorldFrozen(false);

            const DrillDefinition* definition = CurrentDrill();
            if (!definition)
            {
                CompleteRun();
                return;
            }

            m_runner->Begin(*definition);
            if (m_player)
            {
                m_player->Teleport(definition->playerStart);
                m_player->SetControlEnabled(false);
            }

            m_drillTimer.Reset();   // so the HUD reads 00:00.00 until GO
            m_stateTimer = m_introSeconds;
            log::Info("Drill", definition->name + " - " + definition->intro);
            break;
        }
        case GameState::Countdown:
        {
            if (m_player)
            {
                m_player->SetControlEnabled(true);
                m_player->SetCombatEnabled(false); // look only
            }
            m_countdownRemaining = m_countdownSeconds;
            m_countdownShown = static_cast<int>(std::ceil(m_countdownSeconds)) + 1;
            break;
        }
        case GameState::Playing:
        {
            if (m_player)
            {
                m_player->SetControlEnabled(true);
                m_player->SetCombatEnabled(true);
            }
            m_shooting->SetEnabled(true);
            SetWorldFrozen(false);

            m_statsAtDrillStart = m_stats;
            m_drillTimer.Reset();
            m_drillTimer.Start();
            m_runTimer.Start();          // resume; accumulates across the whole run
            m_drillCompletePending = false;

            // Music starts at GO - the actual drill start, not the intro card.
            if (const DrillDefinition* definition = CurrentDrill())
            {
                m_music.Play(definition->musicPath);
            }

            log::Info("Drill", "GO");
            break;
        }
        case GameState::DrillComplete:
        {
            m_music.Stop();
            m_drillTimer.Stop();
            m_runTimer.Stop();
            m_shooting->SetEnabled(false);
            if (m_player) { m_player->SetControlEnabled(false); }
            SetWorldFrozen(true);

            const DrillDefinition* definition = CurrentDrill();

            DrillResult result;
            result.drillId = definition ? definition->id : "";
            result.drillName = definition ? definition->name : "";
            result.timeSeconds = m_drillTimer.ElapsedSeconds();
            result.shotsFired = m_stats.shotsFired - m_statsAtDrillStart.shotsFired;
            result.hits = m_stats.hits - m_statsAtDrillStart.hits;
            result.objectivesCompleted = m_runner->CompletedObjectiveCount();
            result.totalObjectives = static_cast<int>(m_runner->Objectives().size());

            RunSummary drillSummary;
            drillSummary.objectivesCompleted = result.objectivesCompleted;
            drillSummary.targetsHit = result.hits;
            drillSummary.shotsFired = result.shotsFired;
            drillSummary.accuracy = result.shotsFired > 0
                ? static_cast<float>(result.hits) / result.shotsFired : 0.0f;
            drillSummary.completionTimeSeconds = result.timeSeconds;
            drillSummary.parTimeSeconds = definition ? definition->targetTimeSeconds : 0.0;
            result.score = ScoreCalculator::Compute(drillSummary).total;

            m_drillResults.push_back(result);
            m_runner->End();

            m_stateTimer = m_drillCompleteSeconds;

            log::Info("Drill", "Completed '" + result.drillName + "' in " +
                FormatStopwatch(result.timeSeconds) + " | accuracy " +
                std::to_string(result.AccuracyPercent()) + "% | score " +
                std::to_string(result.score));
            break;
        }
        case GameState::RunComplete:
        {
            CompleteRun();
            break;
        }
        case GameState::Paused:
        {
            break; // handled entirely in OnPausePressed
        }
        }
    }

    void CourseDirector::UpdateState(float deltaTime)
    {
        switch (m_state)
        {
        case GameState::MainMenu:
            break;

        case GameState::DrillIntro:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
            {
                EnterState(GameState::Countdown);
            }
            break;

        case GameState::Countdown:
        {
            m_countdownRemaining -= deltaTime;
            const int shown = std::max(0, static_cast<int>(std::ceil(m_countdownRemaining)));
            if (shown < m_countdownShown)
            {
                m_countdownShown = shown;
                log::Info("Countdown", shown > 0 ? std::to_string(shown) : "GO!");
            }
            if (m_countdownRemaining <= 0.0f)
            {
                EnterState(GameState::Playing);
            }
            break;
        }

        case GameState::Playing:
            m_shooting->Update(deltaTime);
            m_runner->Update(deltaTime);
            if (m_drillCompletePending)
            {
                m_drillCompletePending = false;
                CompleteDrill();
            }
            break;

        case GameState::DrillComplete:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
            {
                if (m_currentDrill + 1 < m_drills.size())
                {
                    ++m_currentDrill;
                    EnterState(GameState::DrillIntro);
                }
                else
                {
                    EnterState(GameState::RunComplete);
                }
            }
            break;

        case GameState::RunComplete:
        case GameState::Paused:
            break;
        }
    }

    void CourseDirector::StartRun()
    {
        if (m_drills.empty())
        {
            log::Warn("Course", "cannot start - no drills");
            return;
        }
        ResetRunState();
        m_currentDrill = 0;
        EnterState(GameState::DrillIntro);
    }

    void CourseDirector::BeginDrill(size_t index)
    {
        m_currentDrill = index;
        EnterState(GameState::DrillIntro);
    }

    void CourseDirector::CompleteDrill()
    {
        EnterState(GameState::DrillComplete);
    }

    void CourseDirector::CompleteRun()
    {
        m_state = GameState::RunComplete;

        m_music.Stop();
        m_runTimer.Stop();
        m_shooting->SetEnabled(false);
        if (m_player) { m_player->SetControlEnabled(false); }
        SetWorldFrozen(true);
        if (m_menuCanvas) { m_menuCanvas->SetActive(false); }
        SetMenuInputActive(true);

        RunSummary summary;
        for (const auto& result : m_drillResults)
        {
            summary.objectivesCompleted += result.objectivesCompleted;
        }
        summary.targetsHit = m_stats.hits;
        summary.shotsFired = m_stats.shotsFired;
        summary.accuracy = m_stats.Accuracy();
        summary.completionTimeSeconds = m_runTimer.ElapsedSeconds();
        for (const auto& drill : m_drills)
        {
            summary.parTimeSeconds += drill.targetTimeSeconds;
        }

        m_finalScore = ScoreCalculator::Compute(summary);
        m_finalRecords = m_highScores.Submit(
            m_finalScore.total, summary.completionTimeSeconds, summary.accuracy);

        log::Info("Course", "COURSE COMPLETE");
        log::Info("Score", "time " + FormatStopwatch(summary.completionTimeSeconds) +
            " | hits " + std::to_string(summary.targetsHit) + "/" + std::to_string(summary.shotsFired) +
            " | accuracy " + std::to_string(m_stats.AccuracyPercent()) + "%");
        log::Info("Score", "objective " + std::to_string(m_finalScore.objectiveScore) +
            " + accuracy " + std::to_string(m_finalScore.accuracyBonus) +
            " + time " + std::to_string(m_finalScore.timeBonus) +
            " = " + std::to_string(m_finalScore.total));
        if (m_finalRecords.newHighScore) { log::Info("Score", "NEW HIGH SCORE"); }
        if (m_finalRecords.newBestTime) { log::Info("Score", "NEW BEST TIME"); }
    }

    void CourseDirector::RetryRun()
    {
        ResetRunState();
        m_currentDrill = 0;
        EnterState(GameState::DrillIntro);
    }

    void CourseDirector::ReturnToMenu()
    {
        ResetRunState();
        EnterState(GameState::MainMenu);
    }

    void CourseDirector::ResetRunState()
    {
        m_music.Stop();
        m_runner->End();
        m_stats.Reset();
        m_statsAtDrillStart.Reset();
        m_runTimer.Reset();
        m_drillTimer.Reset();
        m_drillResults.clear();
        m_finalScore = ScoreBreakdown{};
        m_finalRecords = HighScoreStore::SubmitResult{};
        m_drillCompletePending = false;
        m_stateTimer = 0.0f;
    }

    void CourseDirector::OnPausePressed()
    {
        if (m_state == GameState::Playing)
        {
            m_state = GameState::Paused;
            m_music.Pause();
            m_drillTimer.Stop();
            m_runTimer.Stop();
            m_shooting->SetEnabled(false);
            if (m_player) { m_player->SetControlEnabled(false); }
            SetWorldFrozen(true);
            eng::Engine::GetInstance().SetCursorEnabled(true);
            log::Info("State", "Paused");
        }
        else if (m_state == GameState::Paused)
        {
            m_state = GameState::Playing;
            m_music.Resume();
            m_drillTimer.Start();
            m_runTimer.Start();
            m_shooting->SetEnabled(true);
            if (m_player)
            {
                m_player->SetControlEnabled(true);
                m_player->SetCombatEnabled(true);
            }
            SetWorldFrozen(false);
            eng::Engine::GetInstance().SetCursorEnabled(false);
            log::Info("State", "Playing (resumed)");
        }
        else if (m_state == GameState::RunComplete)
        {
            ReturnToMenu();
        }
    }

    void CourseDirector::DebugCompleteActiveDrill()
    {
        if (m_state == GameState::Playing && m_runner)
        {
            m_runner->DebugCompleteAllObjectives();
        }
    }

    void CourseDirector::OnConfirmPressed()
    {
        if (m_state == GameState::RunComplete)
        {
            RetryRun();
        }
        else if (m_state == GameState::DrillComplete)
        {
            m_stateTimer = 0.0f; // skip the remaining dwell
        }
    }
}
