#include "editor/PlacementTool.h"

#include "editor/EditorCamera.h"
#include "editor/EditorCommands.h"
#include "editor/EditorLog.h"

#include "Engine.h"
#include "physics/PhysicsManager.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace editor
{
    namespace
    {
        float SnapValue(float v, float grid)
        {
            return grid > 0.0001f ? std::round(v / grid) * grid : v;
        }
    }

    void PlacementTool::Begin(eng::Scene* scene, nlohmann::json doc, std::string label)
    {
        Abort(scene);
        m_doc = std::move(doc);
        m_label = std::move(label);
        m_active = true;
        m_distance = 8.0f;
        m_yawDegrees = 0.0f;
        m_hasSurface = false;

        nlohmann::json ghost = m_doc;
        ghost["name"] = "(placing) " + m_label;
        ghost.erase("id");
        // The preview must not have a collider, or the crosshair raycast would
        // hit the ghost itself and it would flicker between surface and air.
        std::function<void(nlohmann::json&)> stripPhysics = [&](nlohmann::json& node)
        {
            if (node.contains("components") && node["components"].is_array())
            {
                auto& comps = node["components"];
                comps.erase(std::remove_if(comps.begin(), comps.end(), [](const nlohmann::json& c)
                {
                    return c.value("type", "") == "PhysicsComponent";
                }), comps.end());
            }
            if (node.contains("children") && node["children"].is_array())
            {
                for (auto& child : node["children"]) stripPhysics(child);
            }
        };
        stripPhysics(ghost);
        m_ghostId = SceneEdit::Instantiate(scene, 0, ghost, static_cast<std::size_t>(-1));
        if (m_ghostId == 0)
        {
            EditorLog::Error("placement", "Could not spawn a preview for '" + m_label + "'.");
            m_active = false;
        }
    }

    void PlacementTool::Abort(eng::Scene* scene)
    {
        if (m_ghostId != 0)
        {
            SceneEdit::Destroy(scene, m_ghostId);
            m_ghostId = 0;
        }
        m_active = false;
    }

    PlacementTool::Outcome PlacementTool::Update(eng::Scene* scene, EditorCamera& camera, const Frame& frame)
    {
        Outcome outcome;
        if (!m_active)
        {
            return outcome;
        }

        if (frame.cancel)
        {
            outcome.kind = Outcome::Kind::Cancelled;
            outcome.ghostId = m_ghostId;
            m_ghostId = 0;
            m_active = false;
            return outcome;
        }

        m_distance = std::max(0.5f, m_distance + frame.wheel * 0.5f);
        if (frame.rotateStep)
        {
            m_yawDegrees = std::fmod(m_yawDegrees + 15.0f, 360.0f);
        }

        glm::vec3 origin, dir;
        camera.Ray(frame.ndcCursor, frame.aspect, origin, dir);

        glm::vec3 position = origin + dir * m_distance;
        glm::vec3 surfaceNormal(0.0f, 1.0f, 0.0f);
        m_hasSurface = false;

        auto& physics = eng::Engine::GetInstance().GetPhysicsManager();
        const eng::RaycastHit hit = physics.Raycast(origin, origin + dir * 500.0f);
        if (hit.hasHit && (m_ghostId == 0 || !hit.object || hit.object->GetId() != m_ghostId))
        {
            position = hit.point;
            surfaceNormal = hit.normal;
            m_hasSurface = true;
        }

        if (frame.gridSnap)
        {
            position.x = SnapValue(position.x, frame.gridSize);
            position.y = SnapValue(position.y, frame.gridSize);
            position.z = SnapValue(position.z, frame.gridSize);
        }

        glm::quat rotation = glm::angleAxis(glm::radians(m_yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        if (frame.alignToSurface && m_hasSurface)
        {
            const glm::vec3 up(0.0f, 1.0f, 0.0f);
            if (glm::length(glm::cross(up, surfaceNormal)) > 1e-4f)
            {
                rotation = glm::rotation(up, glm::normalize(surfaceNormal)) * rotation;
            }
        }

        m_lastPosition = position;

        if (auto* ghost = scene ? scene->FindObjectById(m_ghostId) : nullptr)
        {
            ghost->SetPosition(position);
            ghost->SetRotation(rotation);
        }

        if (frame.confirm)
        {
            nlohmann::json finalDoc = m_doc;
            finalDoc["position"] = { { "x", position.x }, { "y", position.y }, { "z", position.z } };
            finalDoc["rotation"] = { { "x", rotation.x }, { "y", rotation.y }, { "z", rotation.z }, { "w", rotation.w } };
            finalDoc.erase("id");

            outcome.kind = Outcome::Kind::Confirmed;
            outcome.doc = std::move(finalDoc);
            outcome.ghostId = m_ghostId;
            m_ghostId = 0;
            m_active = false;
        }

        return outcome;
    }
}
