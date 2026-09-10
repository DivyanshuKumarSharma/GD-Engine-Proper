#include "editor/PrimitiveFactory.h"

#include <algorithm>

namespace editor
{
    namespace
    {
        nlohmann::json Vec3(const glm::vec3& v)
        {
            return { { "x", v.x }, { "y", v.y }, { "z", v.z } };
        }

        nlohmann::json ColourParams(const glm::vec3& c)
        {
            return { { "float3", nlohmann::json::array({
                { { "name", "color" }, { "value0", c.x }, { "value1", c.y }, { "value2", c.z } } }) } };
        }

        nlohmann::json MeshComp(const nlohmann::json& mesh, const glm::vec3& colour)
        {
            return {
                { "type", "MeshComponent" },
                { "mesh", mesh },
                { "material", { { "path", "materials/checker.mat" }, { "params", ColourParams(colour) } } },
            };
        }

        nlohmann::json StaticBoxCollider(const glm::vec3& size, double friction = 0.6)
        {
            return {
                { "type", "PhysicsComponent" },
                { "collider", { { "type", "box" }, { "x", size.x }, { "y", size.y }, { "z", size.z } } },
                { "body", { { "mass", 0 }, { "friction", friction }, { "type", "static" } } },
            };
        }
    }

    const std::vector<PrimitiveFactory::Entry>& PrimitiveFactory::Catalog()
    {
        static const std::vector<Entry> catalog = {
            { "target-circle", "Circle Target", "Targets" },
            { "target-square", "Square Target", "Targets" },
            { "target-triangle", "Triangle Target", "Targets" },
            { "target-cube", "Knock Cube", "Targets" },
            { "target-moving", "Moving Target", "Targets" },
            { "reach-zone", "Reach Zone", "Targets" },

            { "shape-box", "Box", "Shapes" },
            { "shape-sphere", "Sphere", "Shapes" },
            { "shape-cylinder", "Cylinder", "Shapes" },
            { "shape-ramp", "Ramp", "Shapes" },

            { "env-floor", "Floor Tile", "Environment" },
            { "env-wall", "Wall", "Environment" },
            { "env-cover", "Cover Block", "Environment" },
            { "env-light", "Light", "Environment" },
            { "env-spawn", "Player Spawn", "Environment" },
        };
        return catalog;
    }

    nlohmann::json PrimitiveFactory::MakeTarget(const std::string& shape, const glm::vec3& position,
                                               const glm::vec3& colour, float size)
    {
        nlohmann::json mesh;
        if (shape == "square")
        {
            mesh = { { "type", "quad" }, { "width", size }, { "height", size } };
        }
        else if (shape == "triangle")
        {
            mesh = { { "type", "triangle" }, { "radius", size * 0.62f } };
        }
        else // circle
        {
            mesh = { { "type", "disc" }, { "radius", size * 0.5f }, { "segments", 48 } };
        }

        return {
            { "name", "Target" },
            { "position", Vec3(position) },
            { "components", nlohmann::json::array({
                MeshComp(mesh, colour),
                { { "type", "TargetComponent" }, { "kind", "red" }, { "tag", "" },
                  { "despawnDelay", 0.18 }, { "shrinkOnConsume", true } },
            }) },
        };
    }

    nlohmann::json PrimitiveFactory::MakeImageTarget(const std::string& imageRelPath, float aspect,
                                                    const glm::vec3& position, float heightMetres)
    {
        aspect = aspect > 0.01f ? aspect : 1.0f;
        const float width = heightMetres * aspect;

        nlohmann::json material = {
            { "path", "materials/checker.mat" },
            { "params", { { "textures", nlohmann::json::array({
                { { "name", "baseColorTexture" }, { "path", imageRelPath } } }) } } },
        };

        return {
            { "name", "Image Target" },
            { "position", Vec3(position) },
            { "components", nlohmann::json::array({
                { { "type", "MeshComponent" },
                  { "mesh", { { "type", "quad" }, { "width", width }, { "height", heightMetres } } },
                  { "material", material } },
                { { "type", "TargetComponent" }, { "kind", "red" },
                  { "despawnDelay", 0.2 }, { "shrinkOnConsume", true } },
            }) },
            { "meta", { { "imageAspect", aspect }, { "source", imageRelPath },
                        { "transparentAreaHits", false } } },
        };
    }

    nlohmann::json PrimitiveFactory::MakeModelTarget(const std::string& modelRelPath, const glm::vec3& position,
                                                    const glm::vec3& colliderSize)
    {
        // Parent holds the collider + target behaviour; the model is a child so a
        // designer can nudge the mesh inside its (visible, editable) hit box.
        return {
            { "name", "Model Target" },
            { "position", Vec3(position) },
            { "components", nlohmann::json::array({
                StaticBoxCollider(colliderSize),
                { { "type", "TargetComponent" }, { "kind", "red" }, { "despawnDelay", 0.4 },
                  { "shrinkOnConsume", false } },
            }) },
            { "children", nlohmann::json::array({
                { { "name", "Model" }, { "type", "gltf" }, { "path", modelRelPath } },
                // Wireframe-ish collider proxy so the hit box is visible in-editor.
                { { "name", "HitBoxGizmo" },
                  { "components", nlohmann::json::array({
                      MeshComp({ { "type", "box" }, { "x", colliderSize.x }, { "y", colliderSize.y }, { "z", colliderSize.z } },
                               glm::vec3(0.1f, 0.9f, 0.6f)) }) },
                  { "meta", { { "editorOnly", true }, { "colliderProxy", true } } } },
            }) },
        };
    }

    nlohmann::json PrimitiveFactory::Make(const std::string& key, const glm::vec3& p)
    {
        if (key == "target-circle") return MakeTarget("circle", p);
        if (key == "target-square") return MakeTarget("square", p);
        if (key == "target-triangle") return MakeTarget("triangle", p);

        if (key == "target-cube")
        {
            return {
                { "name", "Knock Cube" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 0.9 }, { "y", 0.9 }, { "z", 0.9 } }, glm::vec3(0.85f, 0.85f, 0.9f)),
                    { { "type", "PhysicsComponent" },
                      { "collider", { { "type", "box" }, { "x", 0.9 }, { "y", 0.9 }, { "z", 0.9 } } },
                      { "body", { { "mass", 4 }, { "friction", 0.5 }, { "type", "dynamic" } } } },
                    { { "type", "TargetComponent" }, { "kind", "cube" }, { "hitImpulse", 16 }, { "despawnDelay", 2.5 } },
                }) },
            };
        }
        if (key == "target-moving")
        {
            nlohmann::json doc = MakeTarget("circle", p, glm::vec3(0.95f, 0.55f, 0.1f));
            doc["name"] = "Moving Target";
            doc["components"].push_back({
                { "type", "MovingTargetComponent" },
                { "path", "diagonal" },
                { "pointA", Vec3(glm::vec3(-3.0f, 0.0f, 0.0f)) },
                { "pointB", Vec3(glm::vec3(3.0f, 2.0f, 0.0f)) },
                { "speed", 2.5 }, { "relative", true },
                { "playback", "pingpong" }, { "easing", true }, { "startDelay", 0.0 },
            });
            return doc;
        }
        if (key == "reach-zone")
        {
            return {
                { "name", "Reach Zone" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 3.0 }, { "y", 0.1 }, { "z", 3.0 } }, glm::vec3(0.2f, 0.85f, 0.4f)),
                    { { "type", "PhysicsComponent" },
                      { "collider", { { "type", "box" }, { "x", 3.0 }, { "y", 2.0 }, { "z", 3.0 } } },
                      { "body", { { "mass", 0 }, { "friction", 0.5 }, { "type", "static" }, { "trigger", true } } } },
                    { { "type", "ReachZoneComponent" }, { "tag", "zone" } },
                }) },
            };
        }

        if (key == "shape-box")
            return { { "name", "Box" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 1 }, { "y", 1 }, { "z", 1 } }, glm::vec3(0.7f)),
                    StaticBoxCollider(glm::vec3(1)) }) } };
        if (key == "shape-sphere")
            return { { "name", "Sphere" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({ MeshComp({ { "type", "sphere" }, { "r", 0.5 } }, glm::vec3(0.7f)) }) } };
        if (key == "shape-cylinder")
            return { { "name", "Cylinder" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "cylinder" }, { "radius", 0.5 }, { "height", 1.5 } }, glm::vec3(0.7f)) }) } };
        if (key == "shape-ramp")
        {
            nlohmann::json doc = { { "name", "Ramp" }, { "position", Vec3(p) },
                { "rotation", { { "x", 0.20f }, { "y", 0.0f }, { "z", 0.0f }, { "w", 0.98f } } },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 3 }, { "y", 0.3 }, { "z", 5 } }, glm::vec3(0.55f)),
                    StaticBoxCollider(glm::vec3(3, 0.3f, 5)) }) } };
            return doc;
        }

        if (key == "env-floor")
            return { { "name", "Floor" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 8 }, { "y", 0.5 }, { "z", 8 } }, glm::vec3(0.6f, 0.62f, 0.66f)),
                    StaticBoxCollider(glm::vec3(8, 0.5f, 8), 0.8) }) } };
        if (key == "env-wall")
            return { { "name", "Wall" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 6 }, { "y", 4 }, { "z", 0.4 } }, glm::vec3(0.5f, 0.53f, 0.58f)),
                    StaticBoxCollider(glm::vec3(6, 4, 0.4f), 0.5) }) } };
        if (key == "env-cover")
            return { { "name", "Cover" }, { "position", Vec3(p) },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "box" }, { "x", 1.6 }, { "y", 1.2 }, { "z", 0.4 } }, glm::vec3(0.42f, 0.44f, 0.5f)),
                    StaticBoxCollider(glm::vec3(1.6f, 1.2f, 0.4f)) }) } };
        if (key == "env-light")
            return { { "name", "Light" }, { "position", Vec3(p + glm::vec3(0, 6, 0)) },
                { "components", nlohmann::json::array({
                    { { "type", "LightComponent" }, { "color", { { "r", 1.0 }, { "g", 0.97 }, { "b", 0.92 } } } } }) } };
        if (key == "env-spawn")
            return { { "name", "PlayerSpawn" }, { "position", Vec3(p) },
                { "meta", { { "playerSpawn", true } } },
                { "components", nlohmann::json::array({
                    MeshComp({ { "type", "cylinder" }, { "radius", 0.4 }, { "height", 0.1 } }, glm::vec3(0.2f, 0.7f, 1.0f)) }) } };

        // Fallback: a plain empty object.
        return { { "name", "Object" }, { "position", Vec3(p) } };
    }
}
