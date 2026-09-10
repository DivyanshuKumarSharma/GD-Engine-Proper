#pragma once

#include "util/Signal.h"

#include <glm/vec3.hpp>

class Player;

namespace game
{
    struct PlayerStats;

    // Consumes the player's fired shots each frame and resolves them with a
    // hitscan ray from the camera:
    //   - every shot increments PlayerStats::shotsFired
    //   - a ray that lands on an interactive object (a TargetComponent, or any
    //     dynamic physics body) counts as a hit and applies knockback
    //   - a TargetComponent additionally gets OnHit() so it can emit its event
    // The visible physics projectile the Player still spawns is purely cosmetic.
    class ShootingController
    {
    public:
        ShootingController(Player* player, PlayerStats* stats);

        void SetEnabled(bool enabled) { m_enabled = enabled; }
        void Update(float deltaTime);

        Signal<glm::vec3> onHitConfirmed;   // hit position, for the HUD hit marker

    private:
        void ResolveShot(const glm::vec3& origin, const glm::vec3& direction);

        Player* m_player = nullptr;
        PlayerStats* m_stats = nullptr;
        bool m_enabled = true;
        float m_range = 250.0f;
        float m_knockbackImpulse = 9.0f;
    };
}
