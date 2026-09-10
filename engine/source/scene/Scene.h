#pragma once
#include "scene/GameObject.h"
#include "Common.h"

#include <vector>
#include <string>
#include <memory>

namespace eng
{
    class Scene
    {
    public:
        static void RegisterTypes();
        void Update(float deltaTime);
        void Clear();

        // Submits every active mesh in the scene to the render queue without
        // ticking components. The editor uses this to draw the scene while it is
        // paused for authoring, so gameplay logic never runs on authored state.
        void RenderEditorPreview();

        GameObject* CreateObject(const std::string& name, GameObject* parent = nullptr);
        GameObject* CreateObject(const std::string& type, const std::string& name, GameObject* parent = nullptr);

        template<typename T, typename = typename std::enable_if_t<std::is_base_of_v<GameObject, T>>>
        T* CreateObject(const std::string& name, GameObject* parent = nullptr)
        {
            auto obj = new T();
            obj->SetName(name);
            obj->m_id = GameObject::AllocateId();
            obj->m_scene = this;
            if (m_isUpdating)
            {
                m_objectsToAdd.push_back({ obj, parent });
            }
            else
            {
                SetParent(obj, parent);
            }
            return obj;
        }

        bool SetParent(GameObject* obj, GameObject* parent);
        GameObject* FindObjectByName(const std::string& name);
        GameObject* FindObjectById(uint64_t id) const;
        int CountObjectsByName(const std::string& name) const;

        // Root-level objects, in order. Tools walk this for the hierarchy view.
        const std::vector<std::unique_ptr<GameObject>>& GetRootObjects() const { return m_objects; }

        // Removes an object (root or child) and everything under it right now,
        // releasing its physics bodies. Safe to call outside Scene::Update.
        bool DestroyImmediate(GameObject* object);

        // Moves an object within its sibling list (root list or its parent's
        // children). Index is clamped. Used for hierarchy drag-reorder and to
        // keep rebuilt objects where the user had them.
        bool SetSiblingIndex(GameObject* object, std::size_t index);
        std::size_t GetSiblingIndex(GameObject* object) const;

        const std::string& GetName() const { return m_name; }
        void SetName(const std::string& name) { m_name = name; }
        const std::string& GetActiveCanvasName() const { return m_activeCanvasName; }
        void SetActiveCanvasName(const std::string& name) { m_activeCanvasName = name; }

        void SetMainCamera(GameObject* camera);
        GameObject* GetMainCamera();

        std::vector<LightData> CollectLights();

        static std::shared_ptr<Scene> Load(const std::string& path);
        // Builds a scene from an already-parsed document (same schema as a .sc
        // file). Used for play-mode snapshots and by the editor.
        static std::shared_ptr<Scene> CreateFromDocument(const nlohmann::json& document);
        nlohmann::json Serialize() const;
        bool Save(const std::string& path) const;
        GameObject* InstantiatePrefab(
            const std::string& prefabPath,
            const std::string& name,
            GameObject* parent = nullptr,
            const nlohmann::json& overrides = nlohmann::json::object());

        // Additively instantiate a single object (same JSON schema as scene/prefab
        // objects, including "components", "children" and "prefab") into a live scene.
        // Call outside Scene::Update so the object is parented immediately.
        GameObject* LoadObjectFromJson(
            const nlohmann::json& jsonObject, GameObject* parent = nullptr);

    private:
        void CollectLightsRecursive(GameObject* obj, std::vector<LightData>& out);
        GameObject* LoadObject(
            const nlohmann::json& jsonObject,
            GameObject* parent,
            std::vector<std::string> prefabStack = {});

    private:
        std::string m_name = "Scene";
        std::string m_activeCanvasName;
        std::vector<std::unique_ptr<GameObject>> m_objects;
        std::vector<std::pair<GameObject*, GameObject*>> m_objectsToAdd;
        GameObject* m_mainCamera = nullptr;
        bool m_isUpdating = false;
    };
}
