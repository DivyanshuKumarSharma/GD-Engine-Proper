#include "weapon/WeaponInventory.h"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr float kBloomPerShot = 0.35f;
        constexpr float kBloomMax = 2.0f;
        constexpr float kBloomDecayPerSecond = 2.5f;
        constexpr float kAimSpreadFactor = 0.35f;
        constexpr float kRecoilDegreesPerUnit = 0.9f;
        constexpr float kMaxStep = 0.25f; // a longer stall is treated as 0.25s
    }

    void WeaponInventory::SetLoadout(std::vector<WeaponDef> weapons)
    {
        m_slots.clear();
        for (auto& w : weapons)
        {
            Slot slot;
            slot.ammo = std::max(1, w.magazine);
            slot.def = std::move(w);
            m_slots.push_back(std::move(slot));
        }
        m_active = 0;
        m_cooldown = 0.0f;
        m_reloadRemaining = 0.0f;
        m_reloadDuration = 0.0f;
        m_drawRemaining = 0.0f;
        m_burstRemaining = 0;
        m_bloom = 0.0f;
        m_dryFire = m_reloadStarted = m_reloadFinished = false;
    }

    const WeaponDef* WeaponInventory::Active() const
    {
        return m_active < m_slots.size() ? &m_slots[m_active].def : nullptr;
    }

    const WeaponDef* WeaponInventory::At(std::size_t index) const
    {
        return index < m_slots.size() ? &m_slots[index].def : nullptr;
    }

    int WeaponInventory::AmmoInMagazine() const
    {
        return m_active < m_slots.size() ? m_slots[m_active].ammo : 0;
    }

    int WeaponInventory::MagazineSize() const
    {
        return m_active < m_slots.size() ? std::max(1, m_slots[m_active].def.magazine) : 0;
    }

    bool WeaponInventory::Select(std::size_t index)
    {
        if (index >= m_slots.size() || index == m_active)
        {
            return false;
        }
        m_active = index;
        m_reloadRemaining = 0.0f;   // switching cancels a reload (ammo stays where it was)
        m_burstRemaining = 0;
        m_cooldown = 0.0f;
        m_drawRemaining = kDrawSeconds;
        return true;
    }

    bool WeaponInventory::SelectRelative(int delta)
    {
        const int count = static_cast<int>(m_slots.size());
        if (count < 2 || delta == 0)
        {
            return false;
        }
        const int next = ((static_cast<int>(m_active) + delta) % count + count) % count;
        return Select(static_cast<std::size_t>(next));
    }

    bool WeaponInventory::BeginReload()
    {
        const WeaponDef* w = Active();
        if (!w || IsReloading() || IsDrawing() || m_slots[m_active].ammo >= std::max(1, w->magazine))
        {
            return false;
        }
        m_reloadDuration = std::max(0.05f, w->reloadSeconds);
        m_reloadRemaining = m_reloadDuration;
        m_burstRemaining = 0;
        m_reloadStarted = true;
        return true;
    }

    float WeaponInventory::ReloadProgress() const
    {
        if (!IsReloading() || m_reloadDuration <= 0.0f)
        {
            return 1.0f;
        }
        return std::clamp(1.0f - m_reloadRemaining / m_reloadDuration, 0.0f, 1.0f);
    }

    float WeaponInventory::DrawProgress() const
    {
        return std::clamp(1.0f - m_drawRemaining / kDrawSeconds, 0.0f, 1.0f);
    }

    bool WeaponInventory::CanFire() const
    {
        return Active() && !IsDrawing() && !IsReloading() && m_slots[m_active].ammo > 0;
    }

    void WeaponInventory::FireOne()
    {
        Slot& slot = m_slots[m_active];
        m_lastShotBlooms.push_back(m_bloom);
        --slot.ammo;
        m_bloom = std::min(m_bloom + kBloomPerShot, kBloomMax);
        if (slot.ammo <= 0)
        {
            BeginReload(); // arcade-style: an emptied magazine reloads itself
        }
    }

    int WeaponInventory::Update(float deltaSeconds, bool triggerHeld, bool triggerPressed)
    {
        const float dt = std::clamp(deltaSeconds, 0.0f, kMaxStep);
        m_lastShotBlooms.clear();

        if (m_drawRemaining > 0.0f)
        {
            m_drawRemaining = std::max(0.0f, m_drawRemaining - dt);
        }
        if (m_reloadRemaining > 0.0f)
        {
            m_reloadRemaining -= dt;
            if (m_reloadRemaining <= 0.0f)
            {
                m_reloadRemaining = 0.0f;
                if (m_active < m_slots.size())
                {
                    m_slots[m_active].ammo = std::max(1, m_slots[m_active].def.magazine);
                }
                m_reloadFinished = true;
            }
        }
        m_bloom = std::max(0.0f, m_bloom - kBloomDecayPerSecond * dt);
        m_cooldown -= dt;

        const WeaponDef* w = Active();
        if (!w)
        {
            return 0;
        }
        const float interval = w->FireInterval();
        int fired = 0;

        if (w->fireMode == "burst")
        {
            if (triggerPressed && m_burstRemaining == 0 && m_cooldown <= 0.0f && CanFire())
            {
                m_burstRemaining = kBurstRounds;
            }
            while (m_burstRemaining > 0 && m_cooldown <= 0.0f && CanFire() && fired < kMaxShotsPerUpdate)
            {
                FireOne();
                ++fired;
                --m_burstRemaining;
                m_cooldown += interval;
                if (m_burstRemaining == 0)
                {
                    m_cooldown += interval * (kBurstRecoverIntervals - 1.0f);
                }
            }
            if (m_burstRemaining > 0 && !CanFire())
            {
                m_burstRemaining = 0; // ran dry / started reloading mid-burst
            }
        }
        else if (w->fireMode == "auto")
        {
            while (triggerHeld && m_cooldown <= 0.0f && CanFire() && fired < kMaxShotsPerUpdate)
            {
                FireOne();
                ++fired;
                m_cooldown += interval;
            }
        }
        else // semi
        {
            if (triggerPressed && m_cooldown <= 0.0f && CanFire())
            {
                FireOne();
                fired = 1;
                m_cooldown = interval;
            }
        }

        // Trigger on an empty magazine (e.g. reload cancelled by a switch).
        if (triggerPressed && fired == 0 && !IsReloading() && !IsDrawing() &&
            m_slots[m_active].ammo <= 0)
        {
            m_dryFire = true;
            BeginReload();
        }

        if (!triggerHeld && m_burstRemaining == 0)
        {
            m_cooldown = std::max(m_cooldown, 0.0f); // no banked shots
        }
        return fired;
    }

    float WeaponInventory::CurrentSpreadDegrees(bool aiming) const
    {
        return SpreadForBloom(m_bloom, aiming);
    }

    float WeaponInventory::SpreadForBloom(float bloom, bool aiming) const
    {
        const WeaponDef* w = Active();
        if (!w)
        {
            return 0.0f;
        }
        return w->spreadDegrees * (1.0f + bloom) * (aiming ? kAimSpreadFactor : 1.0f);
    }

    float WeaponInventory::RecoilKickDegrees() const
    {
        const WeaponDef* w = Active();
        return w ? w->recoil * kRecoilDegreesPerUnit : 0.0f;
    }

    bool WeaponInventory::ConsumeDryFire()
    {
        const bool v = m_dryFire;
        m_dryFire = false;
        return v;
    }

    bool WeaponInventory::ConsumeReloadStarted()
    {
        const bool v = m_reloadStarted;
        m_reloadStarted = false;
        return v;
    }

    bool WeaponInventory::ConsumeReloadFinished()
    {
        const bool v = m_reloadFinished;
        m_reloadFinished = false;
        return v;
    }

    float WeaponInventory::RandomSigned()
    {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return dist(m_rng);
    }
}
