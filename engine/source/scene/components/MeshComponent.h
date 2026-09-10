#pragma once

#include "scene/Component.h"
#include <memory>

namespace eng
{
    class Material;
    class Mesh;

    class MeshComponent : public Component
    {
        COMPONENT(MeshComponent)
    public:
        MeshComponent() = default;
        MeshComponent(const std::shared_ptr<Material>& material, const std::shared_ptr<Mesh>& mesh);
        void LoadProperties(const nlohmann::json& json) override;
        void Update(float deltaTime) override;

        // Submits this mesh to the render queue using the owner's current world
        // transform. Update() calls this every frame; the editor calls it
        // directly so the scene can be drawn without ticking gameplay logic.
        void Render();

        void SetMaterial(const std::shared_ptr<Material>& material);
        const std::shared_ptr<Material>& GetMaterial() const;
        void SetMesh(const std::shared_ptr<Mesh>& mesh);
        const std::shared_ptr<Mesh>& GetMesh() const;

    private:
        std::shared_ptr<Material> m_material;
        std::shared_ptr<Mesh> m_mesh;
    };
}