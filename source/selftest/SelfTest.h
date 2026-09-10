#pragma once

#include <set>
#include <string>
#include <vector>

namespace eng
{
    class Scene;
    class GameObject;
}

class Player;

namespace game
{
    class CourseDirector;

    // Headless integration harness (`--selftest`). Run 1 is a real bot
    // playthrough of all three drills - it fires hitscan shots at every target,
    // shoots the crates off / into their zones, and walks the player into each
    // finish trigger, asserting that the objective / event / physics chains all
    // fire for real. Run 2 completes the drills synthetically to exercise retry,
    // mid-drill restart and high-score handling quickly. Then it closes the app.
    class SelfTest
    {
    public:
        SelfTest(eng::Scene* scene, CourseDirector* director, Player* player);

        void SetScreenshotDir(const std::string& dir) { m_screenshotDir = dir; }

        bool Update(float deltaTime);   // true once finished

        bool Finished() const { return m_finished; }
        bool Passed() const { return m_finished && m_failures == 0; }
        int Checks() const { return m_checks; }
        int Failures() const { return m_failures; }

    private:
        enum class Phase
        {
            Boot,
            PlayDrill,
            AwaitNextDrill,
            RunCompleteVerify,
            HoldThenRetry,
            RetryPlaying,
            Retry2Playing,
            Retry2Advance,
            Run2Verify,
            Done
        };
        enum class Step { Shoot, Reach };

        void Check(bool condition, const std::string& label);
        void MaybeScreenshot();

        std::vector<eng::GameObject*> DrillObjects() const;
        void CollectDrillObjects(eng::GameObject* object, std::vector<eng::GameObject*>& out) const;
        void FireAt(eng::GameObject* target);
        void TeleportToVantageOf(eng::GameObject* target);
        bool ShootNextTargetOrCrate(float deltaTime);  // false when nothing left to shoot
        int RemainingReachZones() const;
        bool ForceStuckKnocks();                        // true if it had to intervene

        std::string m_screenshotDir;
        int m_screenshotState = -1;
        int m_screenshotDelay = 0;
        std::set<int> m_capturedKeys;

        eng::Scene* m_scene = nullptr;
        CourseDirector* m_director = nullptr;
        Player* m_player = nullptr;

        Phase m_phase = Phase::Boot;
        Step m_step = Step::Shoot;
        int m_checks = 0;
        int m_failures = 0;
        bool m_finished = false;

        float m_totalTime = 0.0f;
        float m_holdTimer = 0.0f;
        float m_stepTimer = 0.0f;
        float m_crateTimer = 0.0f;
        std::size_t m_crateIndex = 0;
        std::size_t m_drillBeingPlayed = 999;
        double m_capturedRunTime = 0.0;
        bool m_movingRaycastChecked = false;
        bool m_drill1ResultChecked = false;
        bool m_drillVerified = false;
        bool m_forcedThisDrill = false;
        bool m_pausedForScreenshot = false;
    };
}
