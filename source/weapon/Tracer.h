#pragma once

#include <eng.h>

#include <glm/vec3.hpp>

namespace game
{
    // Purely visual shot streak. Deliberately has NO physics body: hit
    // registration is the ShootingController's hitscan alone, so one trigger
    // pull can never score twice (once by ray, once by a physical projectile
    // shoving a knock target) - the inconsistency the legacy rigid-body bullet
    // allowed.
    class Tracer : public eng::GameObject
    {
        GAMEOBJECT(Tracer)
    public:
        void Launch(const glm::vec3& from, const glm::vec3& direction, float speed, float lifetime);
        void Update(float deltaTime) override;

    private:
        glm::vec3 m_velocity = glm::vec3(0.0f);
        float m_lifetime = 0.1f;
    };
}
