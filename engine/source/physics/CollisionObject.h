#pragma once

#include "Common.h"
#include <vector>

namespace eng
{
    class IContactListener;
    class GameObject;

    enum class CollisionObjectType
    {
        RigidBody,
        KinematicCharacterController
    };

    class CollisionObject
    {
    public:
        CollisionObjectType GetCollisionObjectType();

        // Optional back-pointer to the scene object that owns this body. Set by the
        // component that creates the body so raycasts and contact events can resolve
        // back to gameplay code. May be null for bodies created outside the scene graph.
        void SetOwnerObject(GameObject* owner);
        GameObject* GetOwnerObject() const;

        void AddContactListener(IContactListener* listener);
        void RemoveContactListener(IContactListener* listener);

    protected:
        void DispatchContactEvent(
            CollisionObject* obj,
            const glm::vec3& pos,
            const glm::vec3& norm);

        CollisionObjectType m_collisionObjectType;
        std::vector<IContactListener*> m_contactListeners;
        GameObject* m_ownerObject = nullptr;

        friend class PhysicsManager;
    };

    class IContactListener
    {
    public:
        virtual void OnContact(
            CollisionObject* obj,
            const glm::vec3& pos,
            const glm::vec3& norm) = 0;
    };
}