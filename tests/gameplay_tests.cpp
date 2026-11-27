// Pure-logic tests for the runtime gameplay systems: weapon fire timing,
// magazines/reload/switching, spread bloom, moving-target paths, and the key
// binding -> input action translation. No window / OpenGL.
// Built as `gameplay_tests`; run it from the build output.

#include "project/CourseProject.h"
#include "targets/PathMotion.h"
#include "weapon/WeaponInventory.h"

#include <eng.h>

#include <glm/geometric.hpp>

#include <cmath>
#include <cstdio>
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

    bool Near(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f)
    {
        return glm::length(a - b) <= eps;
    }

    game::WeaponDef MakeWeapon(const std::string& mode, float rpm, int magazine, float reload)
    {
        game::WeaponDef w;
        w.id = "test-" + mode;
        w.name = "Test " + mode;
        w.fireMode = mode;
        w.fireRateRpm = rpm;
        w.magazine = magazine;
        w.reloadSeconds = reload;
        w.spreadDegrees = 1.0f;
        w.recoil = 1.0f;
        w.damage = 25.0f;
        return w;
    }

    // Holds the trigger for `seconds` at a fixed frame rate; returns rounds fired.
    int HoldTrigger(game::WeaponInventory& inv, float seconds, float fps)
    {
        const float dt = 1.0f / fps;
        int fired = 0;
        bool first = true;
        for (float t = 0.0f; t < seconds - 1e-6f; t += dt)
        {
            fired += inv.Update(dt, true, first);
            first = false;
        }
        return fired;
    }

    // ------------------------------------------------------------------ weapons
    void TestAutoFireIsFrameRateIndependent()
    {
        // 600 rpm = 10 rounds/s; a 60-round magazine so nothing reloads.
        const auto weapon = MakeWeapon("auto", 600.0f, 60, 1.0f);
        int results[3];
        const float rates[3] = { 30.0f, 60.0f, 144.0f };
        for (int i = 0; i < 3; ++i)
        {
            game::WeaponInventory inv;
            inv.SetLoadout({ weapon });
            results[i] = HoldTrigger(inv, 1.0f, rates[i]);
        }
        // First round at t=0, then every 0.1s -> 10 rounds within [0, 1).
        for (int r : results)
        {
            CHECK(r >= 10 && r <= 11);
        }
        CHECK(std::abs(results[0] - results[2]) <= 1);
        CHECK(std::abs(results[1] - results[2]) <= 1);

        // 1200 rpm at 30 fps (dt > interval) must still fire ~20/s - several per frame.
        game::WeaponInventory fast;
        fast.SetLoadout({ MakeWeapon("auto", 1200.0f, 100, 1.0f) });
        const int fastRounds = HoldTrigger(fast, 1.0f, 30.0f);
        CHECK(fastRounds >= 19 && fastRounds <= 21);
    }

    void TestSemiNeedsAPressPerRound()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("semi", 600.0f, 15, 1.0f) });
        // Holding without re-pressing fires exactly once.
        CHECK(HoldTrigger(inv, 1.0f, 60.0f) == 1);

        // Presses faster than the fire rate are rate-limited, not queued.
        game::WeaponInventory spam;
        spam.SetLoadout({ MakeWeapon("semi", 120.0f, 15, 1.0f) }); // 0.5 s between rounds
        int fired = 0;
        for (int frame = 0; frame < 60; ++frame) // 1 s at 60 fps, pressing every frame
        {
            fired += spam.Update(1.0f / 60.0f, true, true);
        }
        CHECK(fired == 2); // t=0 and t=0.5
    }

    void TestBurstFiresThreeThenPauses()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("burst", 900.0f, 30, 1.0f) }); // 0.0667 s interval
        int fired = inv.Update(1.0f / 60.0f, true, true);
        for (int i = 0; i < 20; ++i) // ~0.33 s, trigger released
        {
            fired += inv.Update(1.0f / 60.0f, false, false);
        }
        CHECK(fired == game::WeaponInventory::kBurstRounds);
        CHECK(inv.AmmoInMagazine() == 30 - game::WeaponInventory::kBurstRounds);
    }

    void TestMagazineAndReload()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("auto", 600.0f, 5, 1.5f) });
        const int fired = HoldTrigger(inv, 0.6f, 60.0f);
        CHECK(fired == 5);                       // magazine empties ...
        CHECK(inv.IsReloading());                // ... and reloads itself
        CHECK(inv.ConsumeReloadStarted());
        CHECK(!inv.ConsumeReloadStarted());      // one-shot flag
        CHECK(inv.Update(0.1f, true, true) == 0); // can't fire mid-reload

        // The 5th round left at t=0.4 s and the reload began then; 0.2 s (rest
        // of the hold) + 0.1 s (above) have elapsed, so ~1.2 s remain of 1.5 s.
        float waited = 0.0f;
        while (inv.IsReloading() && waited < 5.0f)
        {
            inv.Update(1.0f / 60.0f, false, false);
            waited += 1.0f / 60.0f;
        }
        CHECK(!inv.IsReloading());
        CHECK(inv.AmmoInMagazine() == 5);
        CHECK(waited > 1.1f && waited < 1.3f);
        CHECK(inv.ConsumeReloadFinished());

        // Manual reload of a full magazine is refused.
        CHECK(!inv.BeginReload());
    }

    void TestSwitchingCancelsReloadAndDraws()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("auto", 600.0f, 3, 2.0f), MakeWeapon("semi", 300.0f, 8, 1.0f) });
        HoldTrigger(inv, 0.5f, 60.0f);           // empty slot 0 -> reloading
        CHECK(inv.IsReloading());

        CHECK(inv.Select(1));
        CHECK(!inv.IsReloading());               // switch cancels the reload
        CHECK(inv.IsDrawing());
        CHECK(inv.Update(1.0f / 60.0f, true, true) == 0); // can't fire while drawing
        for (int i = 0; i < 20; ++i) inv.Update(1.0f / 60.0f, false, false);
        CHECK(!inv.IsDrawing());
        CHECK(inv.Update(1.0f / 60.0f, true, true) == 1);
        CHECK(inv.AmmoInMagazine() == 7);

        // Back to slot 0: still empty (the reload was cancelled), so the trigger
        // dry-fires and starts a reload.
        CHECK(inv.SelectRelative(-1));
        for (int i = 0; i < 20; ++i) inv.Update(1.0f / 60.0f, false, false);
        CHECK(inv.AmmoInMagazine() == 0);
        CHECK(inv.Update(1.0f / 60.0f, true, true) == 0);
        CHECK(inv.ConsumeDryFire());
        CHECK(inv.IsReloading());

        CHECK(!inv.Select(5));                   // out of range
        CHECK(!inv.Select(inv.ActiveIndex()));   // already active
    }

    void TestSpreadBloomAndRecoil()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("auto", 600.0f, 60, 1.0f) });
        const float rest = inv.CurrentSpreadDegrees(false);
        CHECK(std::fabs(rest - 1.0f) < 1e-5f);
        CHECK(inv.CurrentSpreadDegrees(true) < rest);       // aiming tightens

        HoldTrigger(inv, 0.5f, 60.0f);
        CHECK(inv.CurrentSpreadDegrees(false) > rest);      // sustained fire blooms
        const auto& blooms = inv.LastShotBlooms();
        (void)blooms;

        for (int i = 0; i < 120; ++i) inv.Update(1.0f / 60.0f, false, false);
        CHECK(std::fabs(inv.CurrentSpreadDegrees(false) - rest) < 1e-4f); // recovers

        // The very first round of a string is fired at zero bloom.
        game::WeaponInventory fresh;
        fresh.SetLoadout({ MakeWeapon("auto", 600.0f, 60, 1.0f) });
        CHECK(fresh.Update(1.0f / 60.0f, true, true) == 1);
        CHECK(fresh.LastShotBlooms().size() == 1);
        CHECK(fresh.LastShotBlooms()[0] == 0.0f);
        CHECK(fresh.RecoilKickDegrees() > 0.0f);

        // Deterministic spread samples for a fixed seed, all within [-1, 1].
        game::WeaponInventory a, b;
        a.Seed(7); b.Seed(7);
        for (int i = 0; i < 16; ++i)
        {
            const float x = a.RandomSigned();
            CHECK(x == b.RandomSigned());
            CHECK(x >= -1.0f && x <= 1.0f);
        }
    }

    void TestHugeStallIsBounded()
    {
        game::WeaponInventory inv;
        inv.SetLoadout({ MakeWeapon("auto", 1400.0f, 200, 1.0f) });
        const int fired = inv.Update(10.0f, true, true); // a 10 s hitch
        CHECK(fired <= game::WeaponInventory::kMaxShotsPerUpdate);
        CHECK(fired >= 1);
    }

    // -------------------------------------------------------------------- paths
    void TestPingPongPath()
    {
        game::PathMotion m;
        m.Configure({ { 0, 0, 0 }, { 4, 0, 0 } }, game::PathPlayback::PingPong, 2.0f, false, 0.0f);
        CHECK(std::fabs(m.Length() - 4.0f) < 1e-5f);
        CHECK(std::fabs(m.CycleSeconds() - 4.0f) < 1e-5f);
        CHECK(Near(m.Sample(0.0f), { 0, 0, 0 }));
        CHECK(Near(m.Sample(1.0f), { 2, 0, 0 }));   // constant travel speed: 2 m/s
        CHECK(Near(m.Sample(2.0f), { 4, 0, 0 }));
        CHECK(Near(m.Sample(3.0f), { 2, 0, 0 }));   // coming back
        CHECK(Near(m.Sample(4.0f), { 0, 0, 0 }));
        CHECK(Near(m.Sample(401.0f), { 2, 0, 0 })); // periodic
    }

    void TestLoopAndOncePaths()
    {
        // Unit square circuit, 1 m/s: 4 s around, closing edge included.
        game::PathMotion loop;
        loop.Configure({ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } },
            game::PathPlayback::Loop, 1.0f, false, 0.0f);
        CHECK(std::fabs(loop.CycleSeconds() - 4.0f) < 1e-5f);
        CHECK(Near(loop.Sample(3.5f), { 0, 0.5f, 0 })); // on the closing edge
        CHECK(Near(loop.Sample(4.0f), { 0, 0, 0 }));

        game::PathMotion once;
        once.Configure({ { 0, 0, 0 }, { 0, 3, 0 } }, game::PathPlayback::Once, 1.0f, false, 0.0f);
        CHECK(Near(once.Sample(1.5f), { 0, 1.5f, 0 }));
        CHECK(Near(once.Sample(99.0f), { 0, 3, 0 }));   // stays at the end
        CHECK(once.IsFinished(3.0f));
        CHECK(!once.IsFinished(2.9f));
    }

    void TestDegenerateAndDelay()
    {
        game::PathMotion zero;
        zero.Configure({ { 1, 2, 3 }, { 1, 2, 3 } }, game::PathPlayback::PingPong, 2.0f, true, 0.0f);
        CHECK(zero.IsDegenerate());
        CHECK(Near(zero.Sample(5.0f), { 1, 2, 3 }));     // no NaN, no motion

        game::PathMotion none;
        none.Configure({}, game::PathPlayback::Loop, 2.0f, true, 0.0f);
        CHECK(Near(none.Sample(1.0f), { 0, 0, 0 }));

        game::PathMotion stopped;
        stopped.Configure({ { 0, 0, 0 }, { 2, 0, 0 } }, game::PathPlayback::PingPong, 0.0f, false, 0.0f);
        CHECK(stopped.IsDegenerate());

        game::PathMotion delayed;
        delayed.Configure({ { 0, 0, 0 }, { 2, 0, 0 } }, game::PathPlayback::PingPong, 1.0f, false, 1.0f);
        CHECK(Near(delayed.Sample(0.9f), { 0, 0, 0 }));  // still waiting
        CHECK(Near(delayed.Sample(2.0f), { 1, 0, 0 }));  // 1 s of motion after the delay
    }

    void TestEasingKeepsSegmentTimes()
    {
        // With easing, the target slows into each waypoint but still arrives at
        // exactly the same moments as constant-speed travel.
        const std::vector<glm::vec3> pts = { { 0, 0, 0 }, { 3, 0, 0 }, { 3, 1, 0 } };
        game::PathMotion eased, linear;
        eased.Configure(pts, game::PathPlayback::Once, 1.0f, true, 0.0f);
        linear.Configure(pts, game::PathPlayback::Once, 1.0f, false, 0.0f);
        CHECK(Near(eased.Sample(3.0f), { 3, 0, 0 }));
        CHECK(Near(eased.Sample(4.0f), { 3, 1, 0 }));
        CHECK(Near(eased.Sample(1.5f), linear.Sample(1.5f))); // segment midpoint matches
        CHECK(eased.Sample(0.3f).x < linear.Sample(0.3f).x);  // but it starts slower
    }

    void TestLargeStepMatchesFineStepping()
    {
        game::PathMotion coarse, fine;
        const std::vector<glm::vec3> pts = { { 0, 0, 0 }, { 5, 0, 0 }, { 5, 2, 0 } };
        coarse.Configure(pts, game::PathPlayback::PingPong, 3.0f, true, 0.25f);
        fine.Configure(pts, game::PathPlayback::PingPong, 3.0f, true, 0.25f);
        glm::vec3 a(0.0f), b(0.0f);
        a = coarse.Advance(7.3f);                          // one enormous frame
        for (int i = 0; i < 730; ++i) b = fine.Advance(0.01f);
        CHECK(Near(a, b, 2e-3f));
    }

    void TestPresets()
    {
        CHECK(game::PathMotion::Preset("horizontal", 2.0f).size() == 2);
        CHECK(game::PathMotion::Preset("diagonal", 2.0f)[1] == glm::vec3(2.0f, 2.0f, 0.0f));
        CHECK(game::PathMotion::Preset("box", 1.0f).size() == 4);
        CHECK(game::PathPlaybackFromString("loop") == game::PathPlayback::Loop);
        CHECK(game::PathPlaybackFromString("once") == game::PathPlayback::Once);
        CHECK(game::PathPlaybackFromString("whatever") == game::PathPlayback::PingPong);
        CHECK(std::string(game::ToString(game::PathPlayback::Loop)) == "loop");
    }

    // ----------------------------------------------------------------- bindings
    void TestBindingTokens()
    {
        using KB = game::KeyBindings;
        CHECK(KB::TokenToInputBinding("1") == nlohmann::json({ { "device", "keyboard" }, { "code", "1" } }));
        CHECK(KB::TokenToInputBinding("r")["code"] == "R");
        CHECK(KB::TokenToInputBinding("F5")["code"] == "F5");
        CHECK(KB::TokenToInputBinding("Mouse2") == nlohmann::json({ { "device", "mouse" }, { "code", "RIGHT" } }));
        CHECK(KB::TokenToInputBinding("MouseWheelUp")["code"] == "WHEEL_UP");
        CHECK(KB::TokenToInputBinding("Left Shift")["code"] == "LEFT_SHIFT");
        CHECK(KB::TokenToInputBinding("LeftCtrl")["code"] == "LEFT_CONTROL");
        CHECK(KB::TokenToInputBinding("banana").is_null());
        CHECK(KB::TokenToInputBinding("").is_null());
        CHECK(KB::EngineActionName("weaponSlot1") == "WeaponSlot1");

        KB bindings; // defaults
        std::vector<std::string> unsupported;
        auto actions = bindings.ToInputActions(&unsupported);
        CHECK(unsupported.empty());
        CHECK(actions.contains("WeaponSlot1"));
        CHECK(actions.contains("WeaponNext"));
        CHECK(actions.contains("Reload"));
        CHECK(actions["AimDownSights"][0]["code"] == "RIGHT");

        bindings.Set("reload", "notakey");
        unsupported.clear();
        actions = bindings.ToInputActions(&unsupported);
        CHECK(!actions.contains("Reload"));
        CHECK(unsupported.size() == 1);
    }

    void TestInputMapWheelAndKeys()
    {
        // The engine parses everything the translator can emit, including the
        // new mouse-wheel device codes.
        auto& input = eng::Engine::GetInstance().GetInputManager();
        game::KeyBindings bindings;
        nlohmann::json map = { { "actions", bindings.ToInputActions() } };
        std::string error;
        CHECK(input.LoadActionMap(map.dump(), &error));
        CHECK(error.empty());

        input.ClearStates();
        CHECK(!input.WasActionPressed("WeaponNext"));
        input.AddScroll(1.0f);
        CHECK(input.WasActionPressed("WeaponNext"));
        CHECK(!input.WasActionPressed("WeaponPrev"));
        input.SetSuppressed(true);
        CHECK(!input.WasActionPressed("WeaponNext"));   // suppression covers the wheel too
        input.SetSuppressed(false);
        input.ClearStates();
        CHECK(!input.WasActionPressed("WeaponNext"));   // a notch lasts one frame
        input.AddScroll(-1.0f);
        CHECK(input.WasActionPressed("WeaponPrev"));
        input.ClearStates();

        // F-keys and named keys parse.
        const std::string fkeys = R"({ "actions": { "Debug": [ { "device": "keyboard", "code": "F11" },
                                                         { "device": "keyboard", "code": "CAPS_LOCK" } ] } })";
        CHECK(input.LoadActionMap(fkeys, &error));
        input.ClearActionMap();
    }
}

int main()
{
    TestAutoFireIsFrameRateIndependent();
    TestSemiNeedsAPressPerRound();
    TestBurstFiresThreeThenPauses();
    TestMagazineAndReload();
    TestSwitchingCancelsReloadAndDraws();
    TestSpreadBloomAndRecoil();
    TestHugeStallIsBounded();
    TestPingPongPath();
    TestLoopAndOncePaths();
    TestDegenerateAndDelay();
    TestEasingKeepsSegmentTimes();
    TestLargeStepMatchesFineStepping();
    TestPresets();
    TestBindingTokens();
    TestInputMapWheelAndKeys();

    std::printf("\ngameplay_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
