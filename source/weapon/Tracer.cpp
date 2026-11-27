#include "weapon/Tracer.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game
{
    void Tracer::Launch(const glm::vec3& from, const glm::vec3& direction, float speed, float lifetime)
    {
        glm::vec3 dir = direction;
        if (glm::dot(dir, dir) < 1e-8f)
        {
            dir = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        dir = glm::normalize(dir);
        m_velocity = dir * speed;
        m_lifetime = lifetime;
        SetPosition(from);
        // The streak mesh is authored along local -Z.
        SetRotation(glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void Tracer::Update(float deltaTime)
    {
        eng::GameObject::Update(deltaTime);
        SetPosition(GetPosition() + m_velocity * deltaTime);
        m_lifetime -= deltaTime;
        if (m_lifetime <= 0.0f)
        {
            MarkForDestroy();
        }
    }
}
