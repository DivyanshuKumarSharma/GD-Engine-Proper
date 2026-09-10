#include "editor/EditorCommands.h"

#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <limits>

namespace editor
{
    namespace SceneEdit
    {
        nlohmann::json Capture(eng::GameObject* object)
        {
            return object ? object->Serialize() : nlohmann::json::object();
        }

        uint64_t Instantiate(eng::Scene* scene, uint64_t parentId,
                             const nlohmann::json& doc, std::size_t siblingIndex)
        {
            if (!scene)
            {
                return 0;
            }
            eng::GameObject* parent = parentId ? scene->FindObjectById(parentId) : nullptr;
            eng::GameObject* created = scene->LoadObjectFromJson(doc, parent);
            if (!created)
            {
                return 0;
            }
            if (siblingIndex != std::numeric_limits<std::size_t>::max())
            {
                scene->SetSiblingIndex(created, siblingIndex);
            }
            return created->GetId();
        }

        bool Destroy(eng::Scene* scene, uint64_t id)
        {
            if (!scene)
            {
                return false;
            }
            eng::GameObject* object = scene->FindObjectById(id);
            return object && scene->DestroyImmediate(object);
        }

        uint64_t Rebuild(eng::Scene* scene, uint64_t id, const nlohmann::json& doc)
        {
            if (!scene)
            {
                return 0;
            }
            eng::GameObject* existing = scene->FindObjectById(id);
            if (!existing)
            {
                return 0;
            }
            eng::GameObject* parent = existing->GetParent();
            const uint64_t parentId = parent ? parent->GetId() : 0;
            const std::size_t index = scene->GetSiblingIndex(existing);

            scene->DestroyImmediate(existing);

            nlohmann::json fixed = doc;
            fixed["id"] = id;   // keep identity stable across the rebuild
            return Instantiate(scene, parentId, fixed, index);
        }
    }

    // ------------------------------------------------------------- Transform
    TransformCommand::TransformCommand(eng::Scene* scene, uint64_t id,
        const glm::vec3& oldPos, const glm::quat& oldRot, const glm::vec3& oldScale,
        const glm::vec3& newPos, const glm::quat& newRot, const glm::vec3& newScale)
        : m_scene(scene), m_id(id),
          m_oldPos(oldPos), m_newPos(newPos),
          m_oldRot(oldRot), m_newRot(newRot),
          m_oldScale(oldScale), m_newScale(newScale)
    {
        m_mergeKey = "xform:" + std::to_string(id);
    }

    void TransformCommand::Apply(const glm::vec3& p, const glm::quat& r, const glm::vec3& s)
    {
        if (auto* object = m_scene ? m_scene->FindObjectById(m_id) : nullptr)
        {
            object->SetPosition(p);
            object->SetRotation(r);
            object->SetScale(s);
        }
    }

    void TransformCommand::Redo() { Apply(m_newPos, m_newRot, m_newScale); }
    void TransformCommand::Undo() { Apply(m_oldPos, m_oldRot, m_oldScale); }

    bool TransformCommand::MergeWith(const ICommand& next)
    {
        const auto* other = dynamic_cast<const TransformCommand*>(&next);
        if (!other || other->m_id != m_id)
        {
            return false;
        }
        m_newPos = other->m_newPos;
        m_newRot = other->m_newRot;
        m_newScale = other->m_newScale;
        return true;
    }

    // -------------------------------------------------------- CreateObject
    CreateObjectCommand::CreateObjectCommand(eng::Scene* scene, uint64_t parentId,
                                             nlohmann::json doc, std::string name)
        : m_scene(scene), m_parentId(parentId), m_doc(std::move(doc)), m_name(std::move(name))
    {
    }

    void CreateObjectCommand::Redo()
    {
        // First run: let the scene assign an id, then pin it for later redo.
        if (m_id != 0)
        {
            m_doc["id"] = m_id;
        }
        else
        {
            m_doc.erase("id");
        }
        m_id = SceneEdit::Instantiate(m_scene, m_parentId, m_doc,
                                      m_siblingIndex == static_cast<std::size_t>(-1)
                                          ? std::numeric_limits<std::size_t>::max()
                                          : m_siblingIndex);
        if (m_id != 0)
        {
            m_doc["id"] = m_id;
        }
    }

    void CreateObjectCommand::Undo()
    {
        if (m_id == 0 || !m_scene)
        {
            return;
        }
        if (auto* object = m_scene->FindObjectById(m_id))
        {
            m_siblingIndex = m_scene->GetSiblingIndex(object);
            m_doc = object->Serialize();
        }
        SceneEdit::Destroy(m_scene, m_id);
    }

    // -------------------------------------------------------- DeleteObject
    DeleteObjectCommand::DeleteObjectCommand(eng::Scene* scene, uint64_t id)
        : m_scene(scene), m_id(id)
    {
        if (auto* object = scene ? scene->FindObjectById(id) : nullptr)
        {
            m_doc = object->Serialize();
            m_doc["id"] = id;
            eng::GameObject* parent = object->GetParent();
            m_parentId = parent ? parent->GetId() : 0;
            m_siblingIndex = scene->GetSiblingIndex(object);
        }
        else
        {
            m_id = 0;
        }
    }

    void DeleteObjectCommand::Redo()
    {
        if (auto* object = m_scene ? m_scene->FindObjectById(m_id) : nullptr)
        {
            m_doc = object->Serialize();
            m_doc["id"] = m_id;
            eng::GameObject* parent = object->GetParent();
            m_parentId = parent ? parent->GetId() : 0;
            m_siblingIndex = m_scene->GetSiblingIndex(object);
        }
        SceneEdit::Destroy(m_scene, m_id);
    }

    void DeleteObjectCommand::Undo()
    {
        SceneEdit::Instantiate(m_scene, m_parentId, m_doc, m_siblingIndex);
    }

    // -------------------------------------------------------- ModifyObject
    ModifyObjectCommand::ModifyObjectCommand(eng::Scene* scene, uint64_t id,
        nlohmann::json before, nlohmann::json after, std::string name, std::string mergeKey)
        : m_scene(scene), m_id(id), m_before(std::move(before)), m_after(std::move(after)),
          m_name(std::move(name)), m_mergeKey(std::move(mergeKey))
    {
    }

    void ModifyObjectCommand::Redo() { SceneEdit::Rebuild(m_scene, m_id, m_after); }
    void ModifyObjectCommand::Undo() { SceneEdit::Rebuild(m_scene, m_id, m_before); }

    bool ModifyObjectCommand::MergeWith(const ICommand& next)
    {
        const auto* other = dynamic_cast<const ModifyObjectCommand*>(&next);
        if (!other || other->m_id != m_id || m_mergeKey.empty() || other->m_mergeKey != m_mergeKey)
        {
            return false;
        }
        m_after = other->m_after;
        return true;
    }
}
