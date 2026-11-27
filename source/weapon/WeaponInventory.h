#pragma once

#include "project/CourseProject.h"   // WeaponDef

#include <cstdint>
#include <random>
#include <vector>

namespace game
{
    // Runtime state of the weapons a player carries in a drill: which one is
    // out, its magazine, reload / draw timers, fire cadence, spread bloom.
    // Pure logic (no engine types) so fire timing is unit-testable.
    //
    // Timing model (frame-rate independent):
    //  - a shot needs `cooldown <= 0`; firing adds 60/rpm seconds to the
    //    cooldown rather than resetting it, so the fractional remainder carries
    //    between frames and a held automatic fires the same number of rounds per
    //    second at 30 fps as at 144 fps (several per frame if needed).
    //  - releasing the trigger clamps the cooldown at 0 - no banked bursts.
    //  - semi:  one round per trigger press.
    //    auto:  rounds while held.
    //    burst: one press fires `kBurstRounds` at the fire rate, then waits
    //           `kBurstRecoverIntervals` intervals before the next burst.
    //  - an empty magazine dry-fires (sound cue) and starts a reload; reserve
    //    ammo is unlimited (it's a range).
    class WeaponInventory
    {
    public:
        static constexpr int kBurstRounds = 3;
        static constexpr float kBurstRecoverIntervals = 3.0f;
        static constexpr float kDrawSeconds = 0.22f;
        static constexpr int kMaxShotsPerUpdate = 16;

        void SetLoadout(std::vector<WeaponDef> weapons);
        bool Empty() const { return m_slots.empty(); }
        std::size_t Count() const { return m_slots.size(); }
        std::size_t ActiveIndex() const { return m_active; }
        const WeaponDef* Active() const;
        const WeaponDef* At(std::size_t index) const;
        int AmmoInMagazine() const;
        int MagazineSize() const;

        // Switching cancels a reload and starts a short draw during which the
        // weapon can't fire. Returns true if the active weapon changed.
        bool Select(std::size_t index);
        bool SelectRelative(int delta);   // wraps

        // No-op when full, already reloading, or drawing.
        bool BeginReload();
        bool IsReloading() const { return m_reloadRemaining > 0.0f; }
        float ReloadProgress() const;     // 0..1 while reloading, else 1
        bool IsDrawing() const { return m_drawRemaining > 0.0f; }
        float DrawProgress() const;       // 0..1

        // Advance timers and resolve the trigger. Returns rounds fired this step.
        int Update(float deltaSeconds, bool triggerHeld, bool triggerPressed);

        // Cone half-angle (degrees) a shot fired now would use.
        float CurrentSpreadDegrees(bool aiming) const;
        // Spread for a given bloom level (the bloom each round was fired at is
        // recorded in LastShotBlooms(), so the first round of a burst is as
        // accurate as a single tap).
        float SpreadForBloom(float bloom, bool aiming) const;
        const std::vector<float>& LastShotBlooms() const { return m_lastShotBlooms; }
        // Vertical view kick (degrees) per round for the active weapon.
        float RecoilKickDegrees() const;
        float Bloom() const { return m_bloom; }

        // One-shot event flags for presentation (sound cues); reading clears them.
        bool ConsumeDryFire();
        bool ConsumeReloadStarted();
        bool ConsumeReloadFinished();

        // Random unit-ish offsets in [-1,1] for spread, seeded for determinism.
        void Seed(std::uint32_t seed) { m_rng.seed(seed); }
        float RandomSigned();

    private:
        struct Slot
        {
            WeaponDef def;
            int ammo = 0;
        };

        bool CanFire() const;
        void FireOne();

        std::vector<Slot> m_slots;
        std::size_t m_active = 0;
        float m_cooldown = 0.0f;
        float m_reloadRemaining = 0.0f;
        float m_reloadDuration = 0.0f;
        float m_drawRemaining = 0.0f;
        int m_burstRemaining = 0;
        float m_bloom = 0.0f;
        bool m_dryFire = false;
        bool m_reloadStarted = false;
        bool m_reloadFinished = false;
        std::vector<float> m_lastShotBlooms;
        std::mt19937 m_rng{ 1337u };
    };
}
