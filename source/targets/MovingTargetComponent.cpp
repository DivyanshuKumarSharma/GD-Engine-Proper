#include "targets/MovingTargetComponent.h"
#include "targets/TargetComponent.h"
#include "systems/Navigation.h"
#include <cmath>
#include <stdexcept>

namespace game
{
    namespace
    {
        glm::vec3 ReadVec3(const nlohmann::json& v, const glm::vec3& fallback)
        {
            if (!v.is_object())
            {
                return fallback;
            }
            return glm::vec3(v.value("x", fallback.x), v.value("y", fallback.y), v.value("z", fallback.z));
        }
    }

    void MovingTargetComponent::LoadProperties(const nlohmann::json& json)
    {
        m_points.clear();
        if (json.contains("points") && json["points"].is_array())
        {
            for (const auto& p : json["points"])
            {
                m_points.push_back(ReadVec3(p, glm::vec3(0.0f)));
            }
        }
        else if (json.contains("pointA") || json.contains("pointB"))
        {
            m_points.push_back(json.contains("pointA") ? ReadVec3(json["pointA"], glm::vec3(0.0f)) : glm::vec3(0.0f));
            m_points.push_back(json.contains("pointB") ? ReadVec3(json["pointB"], glm::vec3(0.0f)) : glm::vec3(0.0f));
        }

        m_speed = json.value("speed", m_speed);
        m_relative = json.value("relative", m_relative);
        m_easing = json.value("easing", m_easing);
        m_startDelay = json.value("startDelay", m_startDelay);
        m_playback = PathPlaybackFromString(json.value("playback", std::string("pingpong")));
        m_navigation = json.value("navigation", nlohmann::json());
        if (m_navigation.is_object()) {
            eng::systems::Grid grid;
            grid.width = m_navigation.value("width", 8); grid.height = m_navigation.value("height", 8);
            const float cellSize = m_navigation.value("cellSize", 1.5f);
            if (grid.width < 1 || grid.width > 128 || grid.height < 1 || grid.height > 128 || !std::isfinite(cellSize) || cellSize <= 0)
                throw std::invalid_argument("navigation grid dimensions/cell size invalid");
            grid.blocked.resize(static_cast<std::size_t>(grid.width) * grid.height);
            const auto start = m_navigation.at("start").get<std::vector<int>>();
            const auto goal = m_navigation.at("goal").get<std::vector<int>>();
            if (start.size() != 2 || goal.size() != 2) throw std::invalid_argument("navigation endpoints need [x,y]");
            for (const auto& value : m_navigation.value("blocked", nlohmann::json::array())) {
                const auto cell = value.get<std::vector<int>>();
                if (cell.size() != 2 || cell[0] < 0 || cell[1] < 0 || cell[0] >= grid.width || cell[1] >= grid.height)
                    throw std::invalid_argument("blocked navigation cell outside grid");
                grid.blocked[cell[1] * grid.width + cell[0]] = 1;
            }
            const auto route = eng::systems::FindPath(grid, {start[0],start[1]}, {goal[0],goal[1]});
            m_points.clear();
            for (auto cell : route.cells) m_points.emplace_back((cell.x-start[0])*cellSize,0,(cell.y-start[1])*cellSize);
            m_relative = true;
        }
        Rebuild();
    }

    void MovingTargetComponent::SaveProperties(nlohmann::json& json) const
    {
        nlohmann::json points = nlohmann::json::array();
        for (const auto& p : m_points)
        {
            points.push_back({ { "x", p.x }, { "y", p.y }, { "z", p.z } });
        }
        json["points"] = points;
        json.erase("pointA");
        json.erase("pointB");
        json["speed"] = m_speed;
        json["relative"] = m_relative;
        json["easing"] = m_easing;
        json["startDelay"] = m_startDelay;
        json["playback"] = ToString(m_playback);
        if (m_navigation.is_object()) json["navigation"] = m_navigation;
    }

    void MovingTargetComponent::Rebuild()
    {
        m_motion.Configure(m_points, m_playback, m_speed, m_easing, m_startDelay);
    }

    void MovingTargetComponent::Init()
    {
        // Deliberately do NOT move the owner here (see header).
        m_origin = m_owner->GetPosition();
        m_originCaptured = true;
        m_motion.Restart();
    }

    void MovingTargetComponent::Update(float deltaTime)
    {
        if (auto* target = m_owner->GetComponent<TargetComponent>())
        {
            if (target->IsConsumed())
            {
                return;
            }
        }
        if (m_points.empty())
        {
            return;
        }

        const glm::vec3 local = m_motion.Advance(deltaTime);
        m_owner->SetPosition(m_relative ? m_origin + local : local);
    }
}
