#include "scene/components/MeshComponent.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "render/RenderQueue.h"
#include "scene/GameObject.h"
#include "Engine.h"

namespace eng
{
    MeshComponent::MeshComponent(const std::shared_ptr<Material>& material, const std::shared_ptr<Mesh>& mesh)
        : m_material(material), m_mesh(mesh)
    {
    }

    void MeshComponent::LoadProperties(const nlohmann::json& json)
    {
        if (json.contains("material"))
        {
            auto& matObj = json["material"];
            const std::string path = matObj.value("path", "");
            auto mat = Material::Load(path);
            if (mat && matObj.contains("params"))
            {
                auto& paramsObj = matObj["params"];

                // Floats
                if (paramsObj.contains("float"))
                {
                    for (auto& p : paramsObj["float"])
                    {
                        std::string name = p.value("name", "");
                        float value = p.value("value", 0.0f);
                        mat->SetParam(name, value);
                    }
                }

                // Float2
                if (paramsObj.contains("float2"))
                {
                    for (auto& p : paramsObj["float2"])
                    {
                        std::string name = p.value("name", "");
                        float v0 = p.value("value0", 0.0f);
                        float v1 = p.value("value1", 0.0f);
                        mat->SetParam(name, v0, v1);
                    }
                }

                // Float3
                if (paramsObj.contains("float3"))
                {
                    for (auto& p : paramsObj["float3"])
                    {
                        std::string name = p.value("name", "");
                        float v0 = p.value("value0", 0.0f);
                        float v1 = p.value("value1", 0.0f);
                        float v2 = p.value("value2", 0.0f);
                        mat->SetParam(name, glm::vec3(v0, v1, v2));
                    }
                }

                // Textures
                if (paramsObj.contains("textures"))
                {
                    for (auto& p : paramsObj["textures"])
                    {
                        std::string name = p.value("name", "");
                        std::string texPath = p.value("path", "");
                        auto texture = Texture::Load(texPath);

                        mat->SetParam(name, texture);
                    }
                }
            }
            SetMaterial(mat);
        }

        if (json.contains("mesh"))
        {
            auto& meshObj = json["mesh"];
            const std::string type = meshObj.value("type", "box");
            if (type == "box")
            {
                glm::vec3 extents;
                extents.x = meshObj.value("x", 1.0f);
                extents.y = meshObj.value("y", 1.0f);
                extents.z = meshObj.value("z", 1.0f);
                SetMesh(Mesh::CreateBox(extents));
            }
            else if (type == "sphere")
            {
                float r = meshObj.value("r", 1.0f);
                SetMesh(Mesh::CreateSphere(r, 24, 16));
            }
            else if (type == "quad" || type == "square")
            {
                SetMesh(Mesh::CreateQuad(meshObj.value("width", 1.0f), meshObj.value("height", 1.0f)));
            }
            else if (type == "disc" || type == "circle")
            {
                SetMesh(Mesh::CreatePolygon(meshObj.value("segments", 48), meshObj.value("radius", 0.5f)));
            }
            else if (type == "polygon" || type == "triangle" || type == "ngon")
            {
                const int sides = type == "triangle" ? 3 : meshObj.value("sides", 3);
                SetMesh(Mesh::CreatePolygon(sides, meshObj.value("radius", 0.5f)));
            }
            else if (type == "cylinder")
            {
                SetMesh(Mesh::CreateCylinder(meshObj.value("radius", 0.5f),
                    meshObj.value("height", 1.0f), meshObj.value("sides", 24)));
            }
        }
    }

    void MeshComponent::Update(float deltaTime)
    {
        Render();
    }

    void MeshComponent::Render()
    {
        if (!m_material || !m_mesh)
        {
            return;
        }

        RenderCommand command;
        command.material = m_material.get();
        command.mesh = m_mesh.get();
        command.modelMatrix = GetOwner()->GetWorldTransform();

        auto& renderQueue = Engine::GetInstance().GetRenderQueue();
        renderQueue.Submit(command);
    }

    void MeshComponent::SetMaterial(const std::shared_ptr<Material>& material)
    {
        m_material = material;
    }

    const std::shared_ptr<Material>& MeshComponent::GetMaterial() const
    {
        return m_material;
    }

    const std::shared_ptr<Mesh>& MeshComponent::GetMesh() const
    {
        return m_mesh;
    }

    void MeshComponent::SetMesh(const std::shared_ptr<Mesh>& mesh)
    {
        m_mesh = mesh;
    }
}