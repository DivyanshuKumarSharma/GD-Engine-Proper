#include "selftest/SelfTest.h"

#include "Player.h"
#include "course/CourseDirector.h"
#include "course/GameState.h"
#include "drills/DrillRunner.h"
#include "objectives/Objective.h"
#include "targets/KnockTargetComponent.h"
#include "targets/ReachZoneComponent.h"
#include "targets/TargetComponent.h"
#include "util/Log.h"

#include <eng.h>

#include <algorithm>
#include <cmath>

namespace game
{
    SelfTest::SelfTest(eng::Scene* scene, CourseDirector* director, Player* player)
        : m_scene(scene)
        , m_director(director)
        , m_player(player)
    {
        log::Info("SelfTest", "starting headless integration harness");
    }

    void SelfTest::Check(bool condition, const std::string& label)
    {
        ++m_checks;
        if (condition)
        {
            log::Info("SelfTest", "ok   " + label);
        }
        else
        {
            ++m_failures;
            log::Warn("SelfTest", "FAIL " + label);
        }
    }

    void SelfTest::CollectDrillObjects(eng::GameObject* object, std::vector<eng::GameObject*>& out) const
    {
        out.push_back(object);
        for (const auto& child : object->GetChildren())
        {
            CollectDrillObjects(child.get(), out);
        }
    }

    std::vector<eng::GameObject*> SelfTest::DrillObjects() const
    {
        std::vector<eng::GameObject*> out;
        if (auto* root = m_scene->FindObjectByName("DrillRoot"))
        {
            CollectDrillObjects(root, out);
        }
        return out;
    }

    void SelfTest::FireAt(eng::GameObject* target)
    {
        if (!target)
        {
            return;
        }
        const glm::vec3 eye = m_player->GetWorldPosition();
        const glm::vec3 toTarget = target->GetWorldPosition() - eye;
        if (glm::dot(toTarget, toTarget) < 1e-4f)
        {
            return;
        }
        m_player->InjectShot(eye, glm::normalize(toTarget));
    }

    void SelfTest::TeleportToVantageOf(eng::GameObject* target)
    {
        // The bot is not a pathing agent - it warps to a clear vantage a few
        // metres in front of the target (+z / player side) and fires from there.
        // This proves every target is hittable and the hit / knock / reach chains
        // fire for real; course navigability is a separate, human judgement.
        const glm::vec3 tp = target->GetWorldPosition();
        m_player->Teleport(glm::vec3(
            std::max(-18.0f, std::min(18.0f, tp.x)),
            2.0f,
            std::max(-18.0f, std::min(18.0f, tp.z + 5.0f))));
    }

    bool SelfTest::ShootNextTargetOrCrate(float deltaTime)
    {
        m_crateTimer -= deltaTime;

        for (auto* object : DrillObjects())
        {
            auto* component = object->GetComponent<TargetComponent>();
            if (component && !component->IsConsumed())
            {
                TeleportToVantageOf(object);
                FireAt(object);
                return true;
            }
        }

        std::vector<eng::GameObject*> crates;
        for (auto* object : DrillObjects())
        {
            auto* component = object->GetComponent<KnockTargetComponent>();
            if (component && !component->HasFired())
            {
                crates.push_back(object);
            }
        }
        if (crates.empty())
        {
            return false;
        }

        // One shove per crate per second so it settles inside / off before the
        // next one - otherwise repeated impulses overshoot the target zone.
        if (m_crateTimer <= 0.0f)
        {
            eng::GameObject* crate = crates[m_crateIndex % crates.size()];
            ++m_crateIndex;
            TeleportToVantageOf(crate);
            FireAt(crate);
            m_crateTimer = 1.0f;
        }
        return true;
    }

    int SelfTest::RemainingReachZones() const
    {
        int remaining = 0;
        for (auto* object : DrillObjects())
        {
            if (auto* zone = object->GetComponent<ReachZoneComponent>())
            {
                if (!zone->HasFired())
                {
                    ++remaining;
                }
            }
        }
        return remaining;
    }

    bool SelfTest::ForceStuckKnocks()
    {
        bool forced = false;
        for (auto* object : DrillObjects())
        {
            if (auto* knock = object->GetComponent<KnockTargetComponent>())
            {
                if (!knock->HasFired())
                {
                    knock->DebugForceCondition();
                    forced = true;
                }
            }
        }
        return forced;
    }

    void SelfTest::MaybeScreenshot()
    {
        if (m_screenshotDir.empty())
        {
            return;
        }

        // Key the "one shot per thing" cache by state, plus drill index for the
        // states that repeat once per drill.
        const GameState gs = m_director->State();
        int key = static_cast<int>(gs);
        std::string name = ToString(gs);
        if (gs == GameState::DrillIntro || gs == GameState::Countdown || gs == GameState::Playing)
        {
            const int drill = static_cast<int>(m_director->CurrentDrillIndex());
            key = 100 + static_cast<int>(gs) * 10 + drill;
            name += "_d" + std::to_string(drill + 1);
        }

        // Wait a couple of frames after entering a state (so the HUD populates and
        // the frame is presented), but only save while the state is still the one
        // we started counting on - the headless window can run slow enough to skip
        // a whole state between calls.
        if (key != m_screenshotState)
        {
            m_screenshotState = key;
            m_screenshotDelay = 2;
            return;
        }
        if (m_screenshotDelay > 0 && --m_screenshotDelay == 0 &&
            m_capturedKeys.insert(key).second)
        {
            const std::string path = m_screenshotDir + "/" + name + ".png";
            if (eng::Engine::GetInstance().GetGraphicsAPI().SaveScreenshot(path))
            {
                log::Info("SelfTest", "screenshot " + name);
            }
        }
    }

    bool SelfTest::Update(float deltaTime)
    {
        if (m_finished)
        {
            return true;
        }

        MaybeScreenshot();

        m_totalTime += deltaTime;
        if (m_totalTime > 90.0f)
        {
            Check(false, "harness finished within its time budget (stalled)");
            m_finished = true;
            return true;
        }

        const GameState state = m_director->State();

        // Skip the drill-complete dwell (keep it when capturing screenshots).
        if (state == GameState::DrillComplete && m_screenshotDir.empty())
        {
            m_director->OnConfirmPressed();
        }

        switch (m_phase)
        {
        case Phase::Boot:
            if (state == GameState::MainMenu)
            {
                m_holdTimer += deltaTime;
                if (m_screenshotDir.empty() || m_holdTimer > 1.0f)
                {
                    log::Info("SelfTest", "run 1: bot playthrough of all three drills");
                    m_director->StartRun();
                    m_holdTimer = 0.0f;
                    m_phase = Phase::PlayDrill;
                }
            }
            break;

        case Phase::PlayDrill:
        {
            if (state != GameState::Playing)
            {
                if (!m_drillVerified)
                {
                    m_drillVerified = true;
                    const DrillResult* result = m_director->LastDrillResult();
                    const bool allObjectives = result &&
                        result->totalObjectives >= 3 &&
                        result->objectivesCompleted == result->totalObjectives;

                    if (m_drillBeingPlayed == 1)
                    {
                        Check(!m_forcedThisDrill, "drill 2 finished through its real mechanics");
                        Check(allObjectives, "drill 2 completed all objectives (targets, knock, reach)");
                    }
                    if (m_drillBeingPlayed == 2)
                    {
                        Check(!m_forcedThisDrill, "drill 3 finished through its real mechanics");
                        Check(allObjectives, "drill 3 completed all objectives (targets, moving, knock, reach)");
                    }
                }
                m_phase = Phase::AwaitNextDrill;
                break;
            }

            const std::size_t idx = m_director->CurrentDrillIndex();
            if (idx != m_drillBeingPlayed)
            {
                m_drillBeingPlayed = idx;
                m_step = Step::Shoot;
                m_stepTimer = 0.0f;
                m_crateTimer = 0.0f;
                m_crateIndex = 0;
                m_drillVerified = false;
                m_forcedThisDrill = false;
                m_movingRaycastChecked = (idx != 2);
                log::Info("SelfTest", "playing drill " + std::to_string(idx));

                Check(m_scene->CountObjectsByName("DrillRoot") == 1,
                    "drill " + std::to_string(idx) + " spawned exactly one DrillRoot");

                if (idx == 0)
                {
                    Check(m_director->RunTimeSeconds() < 0.5, "run timer excludes the countdown");
                    if (auto* cube = m_scene->FindObjectByName("D1_Cube_C"))
                    {
                        const glm::vec3 eye = m_player->GetWorldPosition();
                        const glm::vec3 to = eye + glm::normalize(cube->GetWorldPosition() - eye) * 120.0f;
                        const eng::RaycastHit hit =
                            eng::Engine::GetInstance().GetPhysicsManager().Raycast(eye, to);
                        Check(hit.hasHit && hit.object &&
                              hit.object->GetComponent<TargetComponent>() != nullptr,
                              "raycast resolves a target GameObject");
                    }
                }
            }

            m_stepTimer += deltaTime;

            if (m_step == Step::Shoot)
            {
                if (idx == 2 && !m_movingRaycastChecked)
                {
                    m_movingRaycastChecked = true;
                    if (auto* mover = m_scene->FindObjectByName("D3_M2"))
                    {
                        const glm::vec3 eye = m_player->GetWorldPosition();
                        const glm::vec3 to = eye + glm::normalize(mover->GetWorldPosition() - eye) * 140.0f;
                        const eng::RaycastHit hit =
                            eng::Engine::GetInstance().GetPhysicsManager().Raycast(eye, to);
                        auto* target = hit.object ? hit.object->GetComponent<TargetComponent>() : nullptr;
                        Check(target != nullptr && target->Kind() == TargetKind::MovingTarget,
                            "raycast resolves a moving kinematic target");
                    }
                }

                const bool moreToShoot = ShootNextTargetOrCrate(deltaTime);
                if (!moreToShoot && m_stepTimer > 1.2f)
                {
                    m_step = Step::Reach;
                    m_stepTimer = 0.0f;
                }
                else if (moreToShoot && m_stepTimer > 12.0f)
                {
                    if (ForceStuckKnocks())
                    {
                        m_forcedThisDrill = true;
                    }
                    m_step = Step::Reach;
                    m_stepTimer = 0.0f;
                }
            }
            else // Step::Reach
            {
                if (RemainingReachZones() > 0)
                {
                    for (auto* object : DrillObjects())
                    {
                        auto* zone = object->GetComponent<ReachZoneComponent>();
                        if (zone && !zone->HasFired())
                        {
                            m_player->Teleport(object->GetWorldPosition());
                            break;
                        }
                    }
                }

                if (m_stepTimer > 5.0f && !m_director->Runner().IsComplete())
                {
                    log::Warn("SelfTest", "drill " + std::to_string(idx) +
                        " did not finish via real mechanics; forcing");
                    if (ForceStuckKnocks())
                    {
                        m_forcedThisDrill = true;
                    }
                    m_director->DebugCompleteActiveDrill();
                    m_forcedThisDrill = true;
                }
            }
            break;
        }

        case Phase::AwaitNextDrill:
        {
            if (const DrillResult* result = m_director->LastDrillResult())
            {
                if (result->drillId == "drill01" && !m_drill1ResultChecked)
                {
                    m_drill1ResultChecked = true;
                    Check(result->timeSeconds > 0.0, "drill 1 time is greater than zero");
                    Check(result->shotsFired >= 5, "drill 1 counted the injected shots");
                    Check(result->hits >= 5, "drill 1 counted the hits");
                    Check(result->objectivesCompleted == 3, "drill 1 completed all objectives");
                }
            }

            if (state == GameState::Playing && m_director->CurrentDrillIndex() != m_drillBeingPlayed)
            {
                m_phase = Phase::PlayDrill;
            }
            else if (state == GameState::RunComplete)
            {
                m_phase = Phase::RunCompleteVerify;
            }
            break;
        }

        case Phase::RunCompleteVerify:
            if (state == GameState::RunComplete)
            {
                const ScoreBreakdown& score = m_director->FinalScore();
                Check(score.total > 0, "final score is computed");
                Check(score.objectiveScore >= 10000, "all ten objectives scored");
                Check(m_director->FinalRecords().newHighScore, "first run sets a new high score");
                Check(m_director->Records().highScore == score.total, "high score store holds the run total");
                Check(m_director->Records().bestTimeSeconds > 0.0, "a best time is recorded");
                m_capturedRunTime = m_director->RunTimeSeconds();
                m_holdTimer = 0.0f;
                m_phase = Phase::HoldThenRetry;
            }
            break;

        case Phase::HoldThenRetry:
            m_holdTimer += deltaTime;
            if (m_holdTimer > 0.6f)
            {
                Check(std::fabs(m_director->RunTimeSeconds() - m_capturedRunTime) < 1e-6,
                    "the run timer does not advance after the course is complete");
                log::Info("SelfTest", "run 2: synthetic completion + mid-drill restart");

                m_director->RetryRun();
                Check(m_director->State() == GameState::DrillIntro, "retry re-enters DrillIntro");
                Check(m_director->CurrentDrillIndex() == 0, "retry returns to drill 0");
                Check(m_director->Stats().shotsFired == 0, "retry resets the shot count");
                Check(m_director->RunTimeSeconds() < 0.05, "retry resets the run timer");
                Check(m_director->DrillResults().empty(), "retry clears the drill results");
                m_phase = Phase::RetryPlaying;
            }
            break;

        case Phase::RetryPlaying:
            if (state == GameState::Playing && m_director->CurrentDrillIndex() == 0)
            {
                Check(m_scene->CountObjectsByName("DrillRoot") == 1, "one DrillRoot after a full retry");
                if (auto* cube = m_scene->FindObjectByName("D1_Cube_C"))
                {
                    Check(cube->GetWorldPosition().y > 2.0f, "a physics prop respawns at its start height");
                }
                m_director->RetryRun(); // restart again, mid-drill
                Check(m_director->State() == GameState::DrillIntro, "mid-drill restart re-enters DrillIntro");
                Check(m_director->Stats().shotsFired == 0, "mid-drill restart resets the stats");
                m_phase = Phase::Retry2Playing;
            }
            break;

        case Phase::Retry2Playing:
            if (state == GameState::Paused)
            {
                m_holdTimer += deltaTime;
                if (m_holdTimer > 1.2f)
                {
                    m_director->OnPausePressed(); // resume
                }
            }
            else if (state == GameState::Playing && m_director->CurrentDrillIndex() == 0)
            {
                Check(m_scene->CountObjectsByName("DrillRoot") == 1, "one DrillRoot after a mid-drill restart");
                if (!m_screenshotDir.empty() && !m_pausedForScreenshot)
                {
                    m_pausedForScreenshot = true;
                    m_holdTimer = 0.0f;
                    m_director->OnPausePressed(); // -> Paused, for the screenshot
                    break;
                }
                m_director->DebugCompleteActiveDrill();
                m_drillBeingPlayed = 0;
                m_phase = Phase::Retry2Advance;
            }
            break;

        case Phase::Retry2Advance:
        {
            const std::size_t idx = m_director->CurrentDrillIndex();
            if (state == GameState::Playing && idx != m_drillBeingPlayed)
            {
                m_drillBeingPlayed = idx;
                m_director->DebugCompleteActiveDrill();
            }
            else if (state == GameState::RunComplete)
            {
                m_phase = Phase::Run2Verify;
            }
            break;
        }

        case Phase::Run2Verify:
            if (state == GameState::RunComplete)
            {
                Check(!m_director->FinalRecords().newHighScore,
                    "a lower-scoring run does not beat the stored high score");
                Check(m_director->Records().highScore > m_director->FinalScore().total,
                    "the stored high score still exceeds this run");
                m_phase = Phase::Done;
            }
            break;

        case Phase::Done:
            log::Info("SelfTest", "harness complete: " + std::to_string(m_checks) +
                " checks, " + std::to_string(m_failures) + " failures");
            m_finished = true;
            return true;
        }

        return false;
    }
}
