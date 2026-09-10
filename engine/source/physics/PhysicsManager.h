#pragma once

#include <glm/vec3.hpp>

#include <memory>

class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;

namespace eng
{
    class RigidBody;
    class CollisionObject;
    class GameObject;

    struct RaycastHit
    {
        bool hasHit = false;
        glm::vec3 point = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f);
        float distance = 0.0f;
        CollisionObject* collisionObject = nullptr; // the physics body that was hit
        GameObject* object = nullptr;               // its owning scene object, if any
    };

    class PhysicsManager
    {
    public:
        PhysicsManager();
        ~PhysicsManager();

        void Init();
        void Update(float deltaTime);

        void SetPaused(bool paused);
        bool IsPaused() const;

        void AddRigidBody(RigidBody* body);
        void RemoveRigidBody(RigidBody* body);

        // Closest-hit ray query against every collision object in the world.
        RaycastHit Raycast(const glm::vec3& from, const glm::vec3& to) const;

        btDiscreteDynamicsWorld* GetWorld();

    private:
        bool m_paused = false;
        std::unique_ptr<btBroadphaseInterface> m_broadphase;
        std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfig;
        std::unique_ptr<btCollisionDispatcher> m_dispatcher;
        std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
        std::unique_ptr<btDiscreteDynamicsWorld> m_world;
    };
}