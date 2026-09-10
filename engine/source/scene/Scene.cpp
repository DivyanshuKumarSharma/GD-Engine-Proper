#include "scene/Scene.h"
#include "scene/components/AnimationComponent.h"
#include "scene/components/CameraComponent.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"
#include "scene/components/PhysicsComponent.h"
#include "scene/components/PlayerControllerComponent.h"
#include "scene/components/AudioComponent.h"
#include "scene/components/AudioListenerComponent.h"
#include "scene/components/SpriteComponent.h"
#include "scene/components/ui/UIElementComponent.h"
#include "scene/components/ui/CanvasComponent.h"
#include "scene/components/ui/TextComponent.h"
#include "scene/components/ui/ButtonComponent.h"
#include "scene/components/ui/RectTransformComponent.h"
#include "Engine.h"

#include <algorithm>
#include <iostream>

namespace
{
    void MergeJson(nlohmann::json& target, const nlohmann::json& overrides)
    {
        if (target.is_array() && overrides.is_array())
        {
            bool keyedArray = true;
            for (const auto& item : overrides)
            {
                if (!item.is_object() || (!item.contains("type") && !item.contains("name")))
                {
                    keyedArray = false;
                    break;
                }
            }

            if (!keyedArray)
            {
                target = overrides;
                return;
            }

            for (const auto& overrideItem : overrides)
            {
                const char* key = overrideItem.contains("type") ? "type" : "name";
                const std::string value = overrideItem.value(key, "");
                auto targetItem = std::find_if(target.begin(), target.end(),
                    [key, &value](const nlohmann::json& item)
                    {
                        return item.is_object() && item.value(key, "") == value;
                    });

                if (targetItem == target.end())
                {
                    target.push_back(overrideItem);
                }
                else
                {
                    MergeJson(*targetItem, overrideItem);
                }
            }
            return;
        }

        if (!target.is_object() || !overrides.is_object())
        {
            target = overrides;
            return;
        }

        for (const auto& entry : overrides.items())
        {
            if (target.contains(entry.key()))
            {
                MergeJson(target[entry.key()], entry.value());
            }
            else
            {
                target[entry.key()] = entry.value();
            }
        }
    }
}

namespace eng
{
    void Scene::RegisterTypes()
    {
        AnimationComponent::Register();
        CameraComponent::Register();
        LightComponent::Register();
        MeshComponent::Register();
        PhysicsComponent::Register();
        PlayerControllerComponent::Register();
        AudioComponent::Register();
        AudioListenerComponent::Register();
        SpriteComponent::Register();
        UIElementComponent::Register();
        CanvasComponent::Register();
        TextComponent::Register();
        ButtonComponent::Register();
        RectTransformComponent::Register();
    }

    void Scene::Update(float deltaTime)
    {
        m_objects.erase(
            std::remove_if(m_objects.begin(), m_objects.end(),
                [](const std::unique_ptr<GameObject>& obj) { return !obj->IsAlive(); }),
            m_objects.end()
        );

        for (auto& obj : m_objectsToAdd)
        {
            SetParent(obj.first, obj.second);
        }
        m_objectsToAdd.clear();

        m_isUpdating = true;
        for (auto it = m_objects.begin(); it != m_objects.end();)
        {
            if ((*it)->IsAlive())
            {
                (*it)->Update(deltaTime);
                ++it;
            }
            else
            {
                it = m_objects.erase(it);
            }
        }
        m_isUpdating = false;
    }

    void Scene::Clear()
    {
        m_objects.clear();
    }

    GameObject* Scene::CreateObject(const std::string& name, GameObject* parent)
    {
        auto obj = new GameObject();
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

    GameObject* Scene::CreateObject(const std::string& type, const std::string& name, GameObject* parent)
    {
        auto obj = GameObjectFactory::GetInstance().CreateGameObject(type);
        if (obj)
        {
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
        }
        return obj;
    }

    bool Scene::SetParent(GameObject* obj, GameObject* parent)
    {
        bool result = false;
        auto currentParent = obj->GetParent();

        if (parent == nullptr)
        {
            if (currentParent != nullptr)
            {
                auto it = std::find_if(
                    currentParent->m_children.begin(),
                    currentParent->m_children.end(),
                    [obj](const std::unique_ptr<GameObject>& el) {
                        return el.get() == obj;
                    }
                );

                if (it != currentParent->m_children.end())
                {
                    m_objects.push_back(std::move(*it));
                    obj->m_parent = nullptr;
                    currentParent->m_children.erase(it);
                    result = true;
                }
            }
            // No parent currently. This can be in 2 cases
            // 1. The object is in the scene root
            // 2. The object has been just created
            else
            {
                auto it = std::find_if(
                    m_objects.begin(),
                    m_objects.end(),
                    [obj](const std::unique_ptr<GameObject>& el) {
                        return el.get() == obj;
                    }
                );

                if (it == m_objects.end())
                {
                    std::unique_ptr<GameObject> objHolder(obj);
                    m_objects.push_back(std::move(objHolder));
                    result = true;
                }
            }
        }
        // We are trying to add it as a child of another object
        else
        {
            if (currentParent != nullptr)
            {
                auto it = std::find_if(
                    currentParent->m_children.begin(),
                    currentParent->m_children.end(),
                    [obj](const std::unique_ptr<GameObject>& el) {
                        return el.get() == obj;
                    }
                );

                if (it != currentParent->m_children.end())
                {
                    bool found = false;
                    auto currentElement = parent;
                    while (currentElement)
                    {
                        if (currentElement == obj)
                        {
                            found = true;
                            break;
                        }
                        currentElement = currentElement->GetParent();
                    }

                    if (!found)
                    {
                        parent->m_children.push_back(std::move(*it));
                        obj->m_parent = parent;
                        currentParent->m_children.erase(it);
                        result = true;
                    }
                }
            }
            // No parent currently. This can be in 2 cases
            // 1. The object is in the scene root
            // 2. The object has been just created
            else
            {
                auto it = std::find_if(
                    m_objects.begin(),
                    m_objects.end(),
                    [obj](const std::unique_ptr<GameObject>& el) {
                        return el.get() == obj;
                    }
                );

                // The object has been hust created
                if (it == m_objects.end())
                {
                    std::unique_ptr<GameObject> objHolder(obj);
                    parent->m_children.push_back(std::move(objHolder));
                    obj->m_parent = parent;
                    result = true;
                }
                else
                {
                    bool found = false;
                    auto currentElement = parent;
                    while (currentElement)
                    {
                        if (currentElement == obj)
                        {
                            found = true;
                            break;
                        }
                        currentElement = currentElement->GetParent();
                    }

                    if (!found)
                    {
                        parent->m_children.push_back(std::move(*it));
                        obj->m_parent = parent;
                        m_objects.erase(it);
                        result = true;
                    }
                }
            }
        }

        return result;
    }

    GameObject* Scene::FindObjectByName(const std::string& name)
    {
        for (auto& obj : m_objects)
        {
            if (auto child = obj->FindChildByName(name))
            {
                return child;
            }
        }
        return nullptr;
    }

    namespace
    {
        void CountByNameRecursive(GameObject* object, const std::string& name, int& count)
        {
            if (object->GetName() == name)
            {
                ++count;
            }
            for (const auto& child : object->GetChildren())
            {
                CountByNameRecursive(child.get(), name, count);
            }
        }
    }

    int Scene::CountObjectsByName(const std::string& name) const
    {
        int count = 0;
        for (const auto& obj : m_objects)
        {
            CountByNameRecursive(obj.get(), name, count);
        }
        return count;
    }

    namespace
    {
        GameObject* FindByIdRecursive(GameObject* object, uint64_t id)
        {
            if (object->GetId() == id)
            {
                return object;
            }
            for (const auto& child : object->GetChildren())
            {
                if (auto found = FindByIdRecursive(child.get(), id))
                {
                    return found;
                }
            }
            return nullptr;
        }

        void RenderPreviewRecursive(GameObject* object)
        {
            if (!object->IsActive())
            {
                return;
            }
            if (auto mesh = object->GetComponent<MeshComponent>())
            {
                mesh->Render();
            }
            for (const auto& child : object->GetChildren())
            {
                RenderPreviewRecursive(child.get());
            }
        }
    }

    GameObject* Scene::FindObjectById(uint64_t id) const
    {
        if (id == 0)
        {
            return nullptr;
        }
        for (const auto& object : m_objects)
        {
            if (auto found = FindByIdRecursive(object.get(), id))
            {
                return found;
            }
        }
        return nullptr;
    }

    bool Scene::DestroyImmediate(GameObject* object)
    {
        if (!object)
        {
            return false;
        }

        // Drop the main-camera pointer if it lives anywhere inside this subtree.
        if (m_mainCamera)
        {
            for (GameObject* walk = m_mainCamera; walk; walk = walk->GetParent())
            {
                if (walk == object)
                {
                    m_mainCamera = nullptr;
                    break;
                }
            }
        }

        if (GameObject* parent = object->GetParent())
        {
            auto it = std::find_if(parent->m_children.begin(), parent->m_children.end(),
                [object](const std::unique_ptr<GameObject>& el) { return el.get() == object; });
            if (it != parent->m_children.end())
            {
                parent->m_children.erase(it);
                return true;
            }
            return false;
        }

        auto it = std::find_if(m_objects.begin(), m_objects.end(),
            [object](const std::unique_ptr<GameObject>& el) { return el.get() == object; });
        if (it != m_objects.end())
        {
            m_objects.erase(it);
            return true;
        }
        return false;
    }

    void Scene::RenderEditorPreview()
    {
        for (const auto& object : m_objects)
        {
            RenderPreviewRecursive(object.get());
        }
    }

    bool Scene::SetSiblingIndex(GameObject* object, std::size_t index)
    {
        if (!object)
        {
            return false;
        }
        auto& list = object->GetParent() ? object->GetParent()->m_children : m_objects;
        auto it = std::find_if(list.begin(), list.end(),
            [object](const std::unique_ptr<GameObject>& el) { return el.get() == object; });
        if (it == list.end())
        {
            return false;
        }
        index = std::min(index, list.size() - 1);
        std::unique_ptr<GameObject> held = std::move(*it);
        list.erase(it);
        list.insert(list.begin() + static_cast<long>(index), std::move(held));
        return true;
    }

    std::size_t Scene::GetSiblingIndex(GameObject* object) const
    {
        if (!object)
        {
            return 0;
        }
        const auto& list = object->GetParent() ? object->GetParent()->m_children : m_objects;
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].get() == object)
            {
                return i;
            }
        }
        return 0;
    }

    void Scene::SetMainCamera(GameObject* camera)
    {
        m_mainCamera = camera;
    }

    GameObject* Scene::GetMainCamera()
    {
        return m_mainCamera;
    }

    std::vector<LightData> Scene::CollectLights()
    {
        std::vector<LightData> lights;
        for (auto& obj : m_objects)
        {
            CollectLightsRecursive(obj.get(), lights);
        }
        return lights;
    }

    std::shared_ptr<Scene> Scene::Load(const std::string& path)
    {
        try
        {
            const std::string contents = Engine::GetInstance().GetFileSystem().LoadAssetFileText(path);
            if (contents.empty())
            {
                return nullptr;
            }

            const auto json = nlohmann::json::parse(contents);
            return CreateFromDocument(json);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Failed to parse scene '" << path << "': " << exception.what() << std::endl;
            return nullptr;
        }
    }

    std::shared_ptr<Scene> Scene::CreateFromDocument(const nlohmann::json& json)
    {
        try
        {
            if (!json.is_object())
            {
                return nullptr;
            }

            auto result = std::make_shared<Scene>();
            result->m_name = json.value("name", "Scene");

            if (json.contains("objects") && json["objects"].is_array())
            {
                for (const auto& object : json["objects"])
                {
                    if (!result->LoadObject(object, nullptr))
                    {
                        std::cerr << "Failed to load an object while building a scene." << std::endl;
                        return nullptr;
                    }
                }
            }

            if (json.contains("camera"))
            {
                const std::string cameraObjectName = json.value("camera", "");
                if (auto camera = result->FindObjectByName(cameraObjectName))
                {
                    result->SetMainCamera(camera);
                }
            }

            result->m_activeCanvasName = json.value("activeCanvas", "");
            if (auto canvasObject = result->FindObjectByName(result->m_activeCanvasName))
            {
                if (auto component = canvasObject->GetComponent<CanvasComponent>())
                {
                    Engine::GetInstance().GetUIInputSystem().SetCanvas(component);
                }
            }

            return result;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Failed to build scene: " << exception.what() << std::endl;
            return nullptr;
        }
    }

    nlohmann::json Scene::Serialize() const
    {
        nlohmann::json result = {
            { "version", 1 },
            { "name", m_name },
            { "objects", nlohmann::json::array() }
        };

        for (const auto& object : m_objects)
        {
            result["objects"].push_back(object->Serialize());
        }

        if (m_mainCamera)
        {
            result["camera"] = m_mainCamera->GetName();
        }
        if (!m_activeCanvasName.empty())
        {
            result["activeCanvas"] = m_activeCanvasName;
        }
        return result;
    }

    bool Scene::Save(const std::string& path) const
    {
        return Engine::GetInstance().GetFileSystem().SaveAssetFileText(path, Serialize().dump(2));
    }

    GameObject* Scene::InstantiatePrefab(
        const std::string& prefabPath,
        const std::string& name,
        GameObject* parent,
        const nlohmann::json& overrides)
    {
        nlohmann::json instance = {
            { "prefab", prefabPath },
            { "name", name }
        };
        if (!overrides.empty())
        {
            instance["overrides"] = overrides;
        }
        return LoadObject(instance, parent);
    }

    GameObject* Scene::LoadObjectFromJson(const nlohmann::json& jsonObject, GameObject* parent)
    {
        return LoadObject(jsonObject, parent);
    }

    void Scene::CollectLightsRecursive(GameObject* obj, std::vector<LightData>& out)
    {
        if (auto light = obj->GetComponent<LightComponent>())
        {
            LightData data;
            data.color = light->GetColor();
            data.position = obj->GetWorldPosition();
            out.push_back(data);
        }

        for (auto& child : obj->m_children)
        {
            CollectLightsRecursive(child.get(), out);
        }
    }

    GameObject* Scene::LoadObject(
        const nlohmann::json& jsonObject,
        GameObject* parent,
        std::vector<std::string> prefabStack)
    {
        if (!jsonObject.is_object())
        {
            return nullptr;
        }

        nlohmann::json resolvedObject = jsonObject;
        std::string prefabPath;
        nlohmann::json prefabOverrides = nlohmann::json::object();

        if (jsonObject.contains("prefab"))
        {
            prefabPath = jsonObject.value("prefab", "");
            if (prefabPath.empty() ||
                std::find(prefabStack.begin(), prefabStack.end(), prefabPath) != prefabStack.end())
            {
                std::cerr << "Invalid or cyclic prefab reference '" << prefabPath << "'." << std::endl;
                return nullptr;
            }

            const std::string contents = Engine::GetInstance().GetFileSystem().LoadAssetFileText(prefabPath);
            if (contents.empty())
            {
                std::cerr << "Could not load prefab '" << prefabPath << "'." << std::endl;
                return nullptr;
            }

            const auto prefabDocument = nlohmann::json::parse(contents);
            if (!prefabDocument.contains("root") || !prefabDocument["root"].is_object())
            {
                std::cerr << "Prefab '" << prefabPath << "' does not contain a root object." << std::endl;
                return nullptr;
            }

            resolvedObject = prefabDocument["root"];
            prefabOverrides = jsonObject.value("overrides", nlohmann::json::object());
            MergeJson(resolvedObject, prefabOverrides);

            nlohmann::json inlineOverrides = jsonObject;
            inlineOverrides.erase("prefab");
            inlineOverrides.erase("overrides");
            MergeJson(resolvedObject, inlineOverrides);
            prefabStack.push_back(prefabPath);
        }

        const std::string name = resolvedObject.value("name", "Object");

        GameObject* gameObject = nullptr;

        if (resolvedObject.contains("type"))
        {
            const std::string type = resolvedObject.value("type", "");
            if (type == "gltf")
            {
                const std::string path = resolvedObject.value("path", "");
                gameObject = GameObject::LoadGLTF(path, this);
                if (gameObject)
                {
                    gameObject->SetParent(parent);
                    gameObject->SetName(name);
                }
            }
            else
            {
                gameObject = CreateObject(type, name, parent);
            }
        }
        else
        {
            gameObject = CreateObject(name, parent);
        }

        if (!gameObject)
        {
            return nullptr;
        }

        gameObject->m_serializedProperties = resolvedObject;
        gameObject->m_prefabPath = prefabPath;
        gameObject->m_prefabOverrides = prefabOverrides;
        gameObject->SetActive(resolvedObject.value("active", true));

        const uint64_t loadedId = resolvedObject.value("id", static_cast<uint64_t>(0));
        gameObject->SetId(loadedId != 0 ? loadedId : GameObject::AllocateId());

        // Read transform
        if (resolvedObject.contains("position"))
        {
            const auto& posObj = resolvedObject["position"];
            glm::vec3 pos;
            pos.x = posObj.value("x", 0.0f);
            pos.y = posObj.value("y", 0.0f);
            pos.z = posObj.value("z", 0.0f);
            gameObject->SetPosition(pos);
        }

        if (resolvedObject.contains("rotation"))
        {
            const auto& rotObj = resolvedObject["rotation"];
            glm::quat rot;
            rot.x = rotObj.value("x", 0.0f);
            rot.y = rotObj.value("y", 0.0f);
            rot.z = rotObj.value("z", 0.0f);
            rot.w = rotObj.value("w", 1.0f);
            gameObject->SetRotation(rot);
        }

        if (resolvedObject.contains("scale"))
        {
            const auto& scaleObj = resolvedObject["scale"];
            glm::vec3 scale;
            scale.x = scaleObj.value("x", 1.0f);
            scale.y = scaleObj.value("y", 1.0f);
            scale.z = scaleObj.value("z", 1.0f);
            gameObject->SetScale(scale);
        }

        gameObject->LoadProperties(resolvedObject);

        if (resolvedObject.contains("components") && resolvedObject["components"].is_array())
        {
            const auto& components = resolvedObject["components"];
            for (const auto& comp : components)
            {
                const std::string type = comp.value("type", "");
                Component* component = ComponentFactory::GetInstance().CreateComponent(type);
                if (component)
                {
                    component->m_serializedProperties = comp;
                    component->LoadProperties(comp);
                    gameObject->AddComponent(component);
                }
            }
        }

        if (resolvedObject.contains("children") && resolvedObject["children"].is_array())
        {
            const auto& children = resolvedObject["children"];
            for (const auto& child : children)
            {
                if (!LoadObject(child, gameObject, prefabStack))
                {
                    return nullptr;
                }
            }
        }

        gameObject->Init();
        return gameObject;
    }
}
