#pragma once

#include "editor/CommandStack.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace eng
{
    class Scene;
    class GameObject;
}

namespace editor
{
    // Shared scene-edit helpers. Every operation is addressed by stable object
    // id so a command keeps working after an unrelated rebuild.
    namespace SceneEdit
    {
        // Full scene-schema snapshot of an object (transform, components, children).
        nlohmann::json Capture(eng::GameObject* object);

        // Instantiate `doc` under `parentId` (0 == scene root) at `siblingIndex`
        // (SIZE_MAX == append). Returns the new object's id, or 0 on failure.
        uint64_t Instantiate(eng::Scene* scene, uint64_t parentId,
                             const nlohmann::json& doc, std::size_t siblingIndex);

        bool Destroy(eng::Scene* scene, uint64_t id);

        // Rebuild an existing object in place from `doc`, keeping its id, parent
        // and sibling index.
        uint64_t Rebuild(eng::Scene* scene, uint64_t id, const nlohmann::json& doc);
    }

    // Local transform edit. Consecutive edits of the same object merge so a gizmo
    // drag or a slider sweep is a single undo step.
    class TransformCommand : public ICommand
    {
    public:
        TransformCommand(eng::Scene* scene, uint64_t id,
                         const glm::vec3& oldPos, const glm::quat& oldRot, const glm::vec3& oldScale,
                         const glm::vec3& newPos, const glm::quat& newRot, const glm::vec3& newScale);

        void Redo() override;
        void Undo() override;
        std::string Name() const override { return "Transform"; }
        std::string MergeKey() const override { return m_mergeKey; }
        bool MergeWith(const ICommand& next) override;

    private:
        void Apply(const glm::vec3& p, const glm::quat& r, const glm::vec3& s);

        eng::Scene* m_scene;
        uint64_t m_id;
        glm::vec3 m_oldPos, m_newPos;
        glm::quat m_oldRot, m_newRot;
        glm::vec3 m_oldScale, m_newScale;
        std::string m_mergeKey;
    };

    // Create an object from a scene-schema doc.
    class CreateObjectCommand : public ICommand
    {
    public:
        CreateObjectCommand(eng::Scene* scene, uint64_t parentId, nlohmann::json doc,
                            std::string name = "Create Object");

        void Redo() override;
        void Undo() override;
        std::string Name() const override { return m_name; }

        uint64_t CreatedId() const { return m_id; }

    private:
        eng::Scene* m_scene;
        uint64_t m_parentId;
        uint64_t m_id = 0;
        nlohmann::json m_doc;
        std::size_t m_siblingIndex = static_cast<std::size_t>(-1);
        std::string m_name;
    };

    // Delete an object; undo restores it (with descendants) where it was.
    class DeleteObjectCommand : public ICommand
    {
    public:
        DeleteObjectCommand(eng::Scene* scene, uint64_t id);

        void Redo() override;
        void Undo() override;
        std::string Name() const override { return "Delete Object"; }
        bool Valid() const { return m_id != 0; }

    private:
        eng::Scene* m_scene;
        uint64_t m_id;
        uint64_t m_parentId = 0;
        std::size_t m_siblingIndex = 0;
        nlohmann::json m_doc;
    };

    // Replace an object with an edited doc (any property that is not a plain
    // transform). Old/new docs are full snapshots so it always round-trips.
    class ModifyObjectCommand : public ICommand
    {
    public:
        ModifyObjectCommand(eng::Scene* scene, uint64_t id,
                            nlohmann::json before, nlohmann::json after,
                            std::string name, std::string mergeKey = {});

        void Redo() override;
        void Undo() override;
        std::string Name() const override { return m_name; }
        std::string MergeKey() const override { return m_mergeKey; }
        bool MergeWith(const ICommand& next) override;

    private:
        eng::Scene* m_scene;
        uint64_t m_id;
        nlohmann::json m_before;
        nlohmann::json m_after;
        std::string m_name;
        std::string m_mergeKey;
    };
}
