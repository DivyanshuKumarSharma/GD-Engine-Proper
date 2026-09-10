#pragma once

namespace game
{
    // Per-run shooting accuracy. The weapon reports every shot; the
    // ShootingController's raycast decides whether that shot hit a target.
    // Because the raycast is closest-hit only, one shot can never register more
    // than one hit, and Misses() can never go negative.
    struct PlayerStats
    {
        int shotsFired = 0;
        int hits = 0;

        void Reset()
        {
            shotsFired = 0;
            hits = 0;
        }

        void RegisterShot() { ++shotsFired; }
        void RegisterHit()  { ++hits; }

        int Misses() const { return shotsFired - hits; }

        // 0..1. Zero shots is defined as 0 accuracy (never divides by zero).
        float Accuracy() const
        {
            return shotsFired > 0
                ? static_cast<float>(hits) / static_cast<float>(shotsFired)
                : 0.0f;
        }

        // Rounded whole-percent for HUD / results display.
        int AccuracyPercent() const
        {
            return shotsFired > 0
                ? static_cast<int>(Accuracy() * 100.0f + 0.5f)
                : 0;
        }
    };
}
