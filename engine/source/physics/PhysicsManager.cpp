#include "physics/PhysicsManager.h"
#include "physics/RigidBody.h"
#include "physics/CollisionObject.h"
#include "Engine.h"
#include "systems/Profiler.h"
#include <cstdlib>

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

#include <glm/geometric.hpp>

namespace eng
{
    PhysicsManager::PhysicsManager()
    {
    }

    PhysicsManager::~PhysicsManager()
    {
    }

    void PhysicsManager::Init()
    {
        m_broadphase = std::make_unique<btDbvtBroadphase>();
        m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
        m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
        m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
        m_world = std::make_unique<btDiscreteDynamicsWorld>(
            m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_collisionConfig.get()
        );

        m_world->setGravity(btVector3(0, -9.81f, 0));
    }

    void PhysicsManager::SetPaused(bool paused)
    {
        m_paused = paused;
    }

    bool PhysicsManager::IsPaused() const
    {
        return m_paused;
    }

    void PhysicsManager::Update(float deltaTime)
    {
        GD_ZONE("Physics update");
        if (m_paused)
        {
            return;
        }

        const btScalar fixedTimeStep = 1.0f / 60.0f;
        const int maxSubsteps = 4;
        // Exclusive world ownership: no scene update, queries or callbacks run
        // while Bullet steps. This is opt-in scheduling, not parallel Bullet.
        const char* jobsFlag = std::getenv("GD_PHYSICS_JOBS");
        if (jobsFlag && std::string(jobsFlag) == "1") {
            auto step = Engine::GetInstance().GetJobs().Submit([this, deltaTime, maxSubsteps, fixedTimeStep] {
                GD_ZONE("Bullet worker step");
                m_world->stepSimulation(deltaTime, maxSubsteps, fixedTimeStep);
            });
            step.get();
        } else {
            GD_ZONE("Bullet main step");
            m_world->stepSimulation(deltaTime, maxSubsteps, fixedTimeStep);
        }

        // process collisions
        auto dispatcher = m_world->getDispatcher();
        const auto numManifolds = dispatcher->getNumManifolds();
        for (int i = 0; i < numManifolds; ++i)
        {
            auto manifold = dispatcher->getManifoldByIndexInternal(i);
            if (!manifold)
            {
                continue;
            }

            auto bodyA = reinterpret_cast<CollisionObject*>(manifold->getBody0()->getUserPointer());
            auto bodyB = reinterpret_cast<CollisionObject*>(manifold->getBody1()->getUserPointer());

            if (!bodyA || !bodyB)
            {
                continue;
            }

            const auto numContacts = manifold->getNumContacts();
            for (int j = 0; j < numContacts; ++j)
            {
                const auto& point = manifold->getContactPoint(j);
                const glm::vec3 pos(
                    point.m_positionWorldOnB.x(),
                    point.m_positionWorldOnB.y(),
                    point.m_positionWorldOnB.z());
                const glm::vec3 norm(
                    point.m_normalWorldOnB.x(),
                    point.m_normalWorldOnB.y(),
                    point.m_normalWorldOnB.z());

                bodyA->DispatchContactEvent(bodyB, pos, norm);
                bodyB->DispatchContactEvent(bodyA, pos, norm);
            }
        }
    }

    void PhysicsManager::AddRigidBody(RigidBody* body)
    {
        if (!body || !m_world)
        {
            return;
        }

        if (auto rigidBody = body->GetBody())
        {
            m_world->addRigidBody(rigidBody, btBroadphaseProxy::StaticFilter,
                btBroadphaseProxy::AllFilter);
            body->SetAddedToWorld(true);
        }
    }

    void PhysicsManager::RemoveRigidBody(RigidBody* body)
    {
        if (!body || !m_world)
        {
            return;
        }

        if (auto rigidBody = body->GetBody())
        {
            m_world->removeRigidBody(rigidBody);
            body->SetAddedToWorld(false);
        }
    }

    RaycastHit PhysicsManager::Raycast(const glm::vec3& from, const glm::vec3& to) const
    {
        RaycastHit result;
        if (!m_world)
        {
            return result;
        }

        const btVector3 rayFrom(from.x, from.y, from.z);
        const btVector3 rayTo(to.x, to.y, to.z);

        btCollisionWorld::ClosestRayResultCallback callback(rayFrom, rayTo);
        m_world->rayTest(rayFrom, rayTo, callback);

        if (!callback.hasHit())
        {
            return result;
        }

        result.hasHit = true;
        result.point = glm::vec3(
            callback.m_hitPointWorld.x(),
            callback.m_hitPointWorld.y(),
            callback.m_hitPointWorld.z());
        result.normal = glm::vec3(
            callback.m_hitNormalWorld.x(),
            callback.m_hitNormalWorld.y(),
            callback.m_hitNormalWorld.z());
        result.distance = glm::length(result.point - from);

        if (callback.m_collisionObject)
        {
            auto* collisionObject = const_cast<btCollisionObject*>(callback.m_collisionObject);
            result.collisionObject = reinterpret_cast<CollisionObject*>(collisionObject->getUserPointer());
            if (result.collisionObject)
            {
                result.object = result.collisionObject->GetOwnerObject();
            }
        }

        return result;
    }

    btDiscreteDynamicsWorld* PhysicsManager::GetWorld()
    {
        return m_world.get();
    }
}
