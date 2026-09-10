// Pure-logic tests for the FPS training course (no OpenGL / window needed).
// Built as a separate `course_tests` executable; run it from the build output.

#include "course/GameEvent.h"
#include "drills/DrillCatalog.h"
#include "objectives/FinishDrillObjective.h"
#include "objectives/KnockObjective.h"
#include "objectives/ObjectiveFactory.h"
#include "objectives/ReachZoneObjective.h"
#include "objectives/ShootObjective.h"
#include "stats/HighScoreStore.h"
#include "stats/PlayerStats.h"
#include "stats/ScoreCalculator.h"
#include "util/Stopwatch.h"

#include <eng.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void Check(bool condition, const char* expr, const char* file, int line)
    {
        ++g_checks;
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL " << file << ':' << line << "  " << expr << '\n';
        }
    }

#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)

    game::GameEvent ShotEvent(game::TargetKind kind, const std::string& tag = "")
    {
        game::GameEvent e;
        e.type = game::GameEventType::TargetShot;
        e.targetKind = kind;
        e.tag = tag;
        return e;
    }

    game::GameEvent KnockEvent(const std::string& tag)
    {
        game::GameEvent e;
        e.type = game::GameEventType::ObjectKnocked;
        e.tag = tag;
        return e;
    }

    game::GameEvent ZoneEvent(const std::string& tag)
    {
        game::GameEvent e;
        e.type = game::GameEventType::ZoneReached;
        e.tag = tag;
        return e;
    }

    void TestStopwatchFormatting()
    {
        CHECK(game::FormatStopwatch(0.0) == "00:00.00");
        CHECK(game::FormatStopwatch(-5.0) == "00:00.00");
        CHECK(game::FormatStopwatch(62.371) == "01:02.37");
        CHECK(game::FormatStopwatch(9.999) == "00:10.00");   // rounds up
        CHECK(game::FormatStopwatch(125.5) == "02:05.50");
    }

    void TestStopwatchPauseResume()
    {
        game::Stopwatch watch;
        CHECK(watch.ElapsedSeconds() == 0.0);
        watch.Start();
        watch.Stop();
        const double first = watch.ElapsedSeconds();
        CHECK(first >= 0.0);
        watch.Stop();                       // idempotent
        CHECK(watch.ElapsedSeconds() == first);
        watch.Reset();
        CHECK(watch.ElapsedSeconds() == 0.0);
        CHECK(!watch.IsRunning());
    }

    void TestPlayerStats()
    {
        game::PlayerStats stats;
        CHECK(stats.Accuracy() == 0.0f);          // zero shots -> defined, no divide by zero
        CHECK(stats.AccuracyPercent() == 0);
        CHECK(stats.Misses() == 0);

        stats.RegisterShot();
        stats.RegisterShot();
        stats.RegisterShot();
        stats.RegisterShot();
        stats.RegisterHit();
        stats.RegisterHit();
        stats.RegisterHit();
        CHECK(stats.Misses() == 1);
        CHECK(std::fabs(stats.Accuracy() - 0.75f) < 1e-6f);
        CHECK(stats.AccuracyPercent() == 75);

        stats.Reset();
        CHECK(stats.shotsFired == 0 && stats.hits == 0);
    }

    void TestScoreCalculator()
    {
        // Mirrors the design doc's worked example.
        game::RunSummary summary;
        summary.objectivesCompleted = 9;
        summary.targetsHit = 21;
        summary.shotsFired = 25;
        summary.accuracy = 0.84f;
        summary.completionTimeSeconds = 102.38;
        summary.parTimeSeconds = 126.38;      // 24s under par

        const game::ScoreBreakdown breakdown = game::ScoreCalculator::Compute(summary);
        CHECK(breakdown.objectiveScore == 9000);
        CHECK(breakdown.accuracyBonus == 1680);
        CHECK(breakdown.timeBonus == 2400);
        CHECK(breakdown.total == 13080);

        // Over par -> no negative time bonus.
        summary.completionTimeSeconds = 500.0;
        CHECK(game::ScoreCalculator::Compute(summary).timeBonus == 0);
    }

    void TestObjectiveCountingAndFilters()
    {
        // Counts once, clamps at target, ignores the wrong event type.
        game::ShootObjective shootAny("Hit Targets", 3);
        CHECK(shootAny.Handle(ShotEvent(game::TargetKind::Cube)));
        CHECK(shootAny.Handle(ShotEvent(game::TargetKind::RedTarget)));
        CHECK(!shootAny.Handle(KnockEvent("x")));            // wrong type - not accepted
        CHECK(shootAny.Handle(ShotEvent(game::TargetKind::Generic)));
        CHECK(shootAny.IsComplete());
        CHECK(!shootAny.Handle(ShotEvent(game::TargetKind::Cube)));   // already complete
        CHECK(shootAny.CurrentAmount() == 3);

        // Kind filter.
        game::ShootObjective reds("Shoot Red", 2, game::TargetKind::RedTarget);
        CHECK(!reds.Handle(ShotEvent(game::TargetKind::Cube)));
        CHECK(reds.Handle(ShotEvent(game::TargetKind::RedTarget)));
        CHECK(reds.CurrentAmount() == 1);

        // Tag filter.
        game::ShootObjective tagged("Tagged", 1, game::TargetKind::Generic, "d1");
        CHECK(!tagged.Handle(ShotEvent(game::TargetKind::Generic, "other")));
        CHECK(tagged.Handle(ShotEvent(game::TargetKind::Generic, "d1")));

        // Moving-target objective is just a kind filter.
        game::ShootObjective moving("Moving", 1, game::TargetKind::MovingTarget);
        CHECK(!moving.Handle(ShotEvent(game::TargetKind::Cube)));
        CHECK(moving.Handle(ShotEvent(game::TargetKind::MovingTarget)));

        // Knock objective.
        game::KnockObjective knock("Knock", 2, "d2_platform");
        CHECK(!knock.Handle(ShotEvent(game::TargetKind::Cube)));
        CHECK(!knock.Handle(KnockEvent("other")));
        CHECK(knock.Handle(KnockEvent("d2_platform")));
        CHECK(knock.Handle(KnockEvent("d2_platform")));
        CHECK(knock.IsComplete());

        // Reach-zone objective: single step, tag must match, no counter shown.
        game::ReachZoneObjective reach("Finish", "d2_finish");
        CHECK(!reach.ShowsCounter());
        CHECK(!reach.Handle(ZoneEvent("wrong")));
        CHECK(reach.Handle(ZoneEvent("d2_finish")));
        CHECK(reach.IsComplete());

        // Finish-drill objective never consumes an event; it is force-completed.
        game::FinishDrillObjective finish("Complete the Drill");
        CHECK(!finish.Handle(ShotEvent(game::TargetKind::Cube)));
        CHECK(!finish.IsComplete());
        finish.ForceComplete();
        CHECK(finish.IsComplete());
    }

    void TestObjectiveFactory()
    {
        game::ObjectiveDefinition shoot;
        shoot.type = "shoot";
        shoot.description = "Shoot Cubes";
        shoot.amount = 3;
        shoot.targetKind = "cube";
        auto shootObjective = game::ObjectiveFactory::Create(shoot);
        CHECK(shootObjective != nullptr);
        CHECK(shootObjective->Kind() == game::ObjectiveKind::Shoot);
        CHECK(shootObjective->TargetAmount() == 3);

        game::ObjectiveDefinition bad;
        bad.type = "nonsense";
        CHECK(game::ObjectiveFactory::Create(bad) == nullptr);

        CHECK(game::TargetKindFromString("cube") == game::TargetKind::Cube);
        CHECK(game::TargetKindFromString("red") == game::TargetKind::RedTarget);
        CHECK(game::TargetKindFromString("moving") == game::TargetKind::MovingTarget);
        CHECK(game::TargetKindFromString("???") == game::TargetKind::Generic);
    }

    void TestHighScoreStore()
    {
        const auto path = std::filesystem::temp_directory_path() / "gd_course_scores_test.json";
        std::error_code ec;
        std::filesystem::remove(path, ec);

        {
            game::HighScoreStore store(path);
            store.Load();
            CHECK(store.Get().highScore == 0);
            CHECK(store.Get().bestTimeSeconds == 0.0);

            auto first = store.Submit(10000, 120.0, 0.8f);
            CHECK(first.newHighScore && first.newBestTime && first.newBestAccuracy);

            auto worse = store.Submit(5000, 200.0, 0.5f);
            CHECK(!worse.newHighScore && !worse.newBestTime && !worse.newBestAccuracy);

            auto betterTime = store.Submit(9000, 90.0, 0.7f);
            CHECK(!betterTime.newHighScore);
            CHECK(betterTime.newBestTime);
        }

        {
            // Survives a reload.
            game::HighScoreStore reloaded(path);
            reloaded.Load();
            CHECK(reloaded.Get().highScore == 10000);
            CHECK(std::fabs(reloaded.Get().bestTimeSeconds - 90.0) < 1e-6);
            CHECK(std::fabs(reloaded.Get().bestAccuracy - 0.8f) < 1e-6f);
        }

        {
            // Corrupt file -> defaults, no crash.
            eng::Engine::GetInstance().GetFileSystem().SaveTextFile(path, "{ this is not json");
            game::HighScoreStore corrupt(path);
            corrupt.Load();
            CHECK(corrupt.Get().highScore == 0);
        }

        std::filesystem::remove(path, ec);
    }

    void TestDrillCatalog()
    {
        const auto drills = game::DrillCatalog::Load();
        CHECK(drills.size() == 3);
        if (drills.size() == 3)
        {
            CHECK(drills[0].id == "drill01");
            CHECK(drills[1].id == "drill02");
            CHECK(drills[2].id == "drill03");

            for (const auto& drill : drills)
            {
                CHECK(!drill.name.empty());
                CHECK(!drill.objectives.empty());
                CHECK(drill.targetTimeSeconds > 0.0f);
                CHECK(drill.content.is_array());
                CHECK(!drill.content.empty());
                for (const auto& objective : drill.objectives)
                {
                    CHECK(game::ObjectiveFactory::Create(objective) != nullptr);
                }
            }

            // Drill 1 matches the spec: 3 cubes, 2 red, finish.
            CHECK(drills[0].objectives.size() == 3);
            CHECK(drills[0].objectives[0].targetKind == "cube");
            CHECK(drills[0].objectives[0].amount == 3);
            CHECK(drills[0].objectives[1].targetKind == "red");
            CHECK(drills[0].objectives[1].amount == 2);
            CHECK(drills[0].objectives[2].type == "finish");
        }
    }
}

int main()
{
    TestStopwatchFormatting();
    TestStopwatchPauseResume();
    TestPlayerStats();
    TestScoreCalculator();
    TestObjectiveCountingAndFilters();
    TestObjectiveFactory();
    TestHighScoreStore();
    TestDrillCatalog();

    std::printf("\ncourse_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
