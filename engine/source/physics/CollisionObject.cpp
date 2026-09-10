#include "physics/CollisionObject.h"

#include <algorithm>

namespace eng
{
    CollisionObjectType CollisionObject::GetCollisionObjectType()
    {
        return m_collisionObjectType;
    }

    void CollisionObject::SetOwnerObject(GameObject* owner)
    {
        m_ownerObject = owner;
    }

    GameObject* CollisionObject::GetOwnerObject() const
    {
        return m_ownerObject;
    }

    void CollisionObject::AddContactListener(IContactListener* listener)
    {
        m_contactListeners.push_back(listener);
    }

    void CollisionObject::RemoveContactListener(IContactListener* listener)
    {
        auto it = std::find(m_contactListeners.begin(), m_contactListeners.end(), listener);
        if (it != m_contactListeners.end())
        {
            m_contactListeners.erase(it);
        }
    }

    void CollisionObject::DispatchContactEvent(
        CollisionObject* obj,
        const glm::vec3& pos,
        const glm::vec3& norm)
    {
        for (auto listener : m_contactListeners)
        {
            if (listener)
            {
                listener->OnContact(obj, pos, norm);
            }
        }
    }
}