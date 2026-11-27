#include "editor/PrimitiveFactory.h"

#include "targets/PathMotion.h"
#include "systems/Navigation.h"

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
            { "target-armored", "Armored Target (100 hp)", "Targets" },
            { "target-moving", "Moving Target", "Targets" },
            { "target-moving-loop", "Circuit Target (loop)", "Targets" },
            { "target-navigation", "A* Navigation Target", "Targets" },
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
        glm::vec3 hitBox(size, size, 0.08f);
        if (shape == "square")
        {
            mesh = { { "type", "quad" }, { "width", size }, { "height", size } };
        }
        else if (shape == "triangle")
        {
            const float r = size * 0.62f;
            mesh = { { "type", "triangle" }, { "radius", r } };
            // Bounding box of an apex-up triangle inscribed in r: width r*sqrt(3), height 1.5r.
            hitBox = glm::vec3(r * 1.732f, r * 1.5f, 0.08f);
        }
        else // circle
        {
            mesh = { { "type", "disc" }, { "radius", size * 0.5f }, { "segments", 48 } };
        }

        // The hit volume is the shape's bounding box (hitscan needs a physics
        // collider; without one a placed target could never be shot). For the
        // triangle it is centred on the bounding box, which sits slightly below
        // the mesh origin.
        nlohmann::json collider = StaticBoxCollider(hitBox, 0.5);
        return {
            { "name", "Target" },
            { "position", Vec3(position) },
            { "components", nlohmann::json::array({
                MeshComp(mesh, colour),
                collider,
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
                StaticBoxCollider(glm::vec3(width, heightMetres, 0.08f), 0.5),
                { { "type", "TargetComponent" }, { "kind", "red" },
                  { "despawnDelay", 0.2 }, { "shrinkOnConsume", true } },
            }) },
            // Explicit, and true: the hit area is the whole image rectangle, so
            // fully transparent pixels still register a hit. Per-pixel alpha
            // testing is not implemented.
            { "meta", { { "imageAspect", aspect }, { "source", imageRelPath },
                        { "transparentAreaHits", true }, { "hitArea", "full-rectangle" } } },
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
        if (key == "target-navigation") {
            auto doc = Make("target-moving", p);
            doc["name"] = "A* Navigation Target";
            for (auto& comp : doc["components"]) if (comp.value("type", "") == "MovingTargetComponent") {
                comp["navigation"] = {{"width",8},{"height",8},{"cellSize",1.5},
                    {"start",{0,0}},{"goal",{7,7}},
                    {"blocked",nlohmann::json::array({{3,0},{3,1},{3,2},{3,3},{3,4},{3,5}})}};
                comp["easing"] = false;
                comp["path"] = "astar";
            }
            return doc;
        }
        if (key == "target-moving" || key == "target-moving-loop")
        {
            const bool loop = key == "target-moving-loop";
            nlohmann::json doc = MakeTarget("circle", p, glm::vec3(0.95f, 0.55f, 0.1f));
            doc["name"] = loop ? "Circuit Target" : "Moving Target";
            nlohmann::json points = nlohmann::json::array();
            for (const auto& wp : game::PathMotion::Preset(loop ? "box" : "diagonal", 3.0f))
            {
                points.push_back(Vec3(wp));
            }
            // Make the target's collider kinematic so it follows the moving mesh
            // and the hitscan ray tracks it.
            for (auto& c : doc["components"])
            {
                if (c.value("type", "") == "PhysicsComponent")
                {
                    c["body"]["type"] = "kinematic";
                }
            }
            doc["components"].push_back({
                { "type", "MovingTargetComponent" },
                { "path", loop ? "box" : "diagonal" },
                { "points", points },
                { "speed", 2.5 }, { "relative", true },
                { "playback", loop ? "loop" : "pingpong" }, { "easing", !loop }, { "startDelay", 0.0 },
            });
            return doc;
        }
        if (key == "target-armored")
        {
            nlohmann::json doc = MakeTarget("square", p, glm::vec3(0.45f, 0.5f, 0.62f), 1.4f);
            doc["name"] = "Armored Target";
            for (auto& c : doc["components"])
            {
                if (c.value("type", "") == "TargetComponent")
                {
                    c["health"] = 100; // a sniper one-shots it; an SMG needs several hits
                    c["despawnDelay"] = 0.35;
                }
            }
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
