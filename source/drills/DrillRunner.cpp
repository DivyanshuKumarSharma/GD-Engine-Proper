#include "drills/DrillRunner.h"

#include "objectives/ObjectiveFactory.h"
#include "targets/KnockTargetComponent.h"
#include "targets/ReachZoneComponent.h"
#include "targets/TargetComponent.h"
#include "util/Log.h"

#include <eng.h>

namespace game
{
    DrillRunner::DrillRunner(eng::Scene* scene)
        : m_scene(scene)
    {
    }

    void DrillRunner::Begin(const DrillDefinition& definition)
    {
        End();

        m_definition = &definition;
        m_completeFired = false;

        // DrillRoot is a plain top-level object; everything the drill spawns hangs
        // off it so End() can drop the whole subtree in one MarkForDestroy().
        m_drillRoot = m_scene->CreateObject("DrillRoot");
        for (const auto& objectJson : definition.content)
        {
            m_scene->LoadObjectFromJson(objectJson, m_drillRoot);
        }

        ConnectEmitters(m_drillRoot);

        for (const auto& objectiveDefinition : definition.objectives)
        {
            if (auto objective = ObjectiveFactory::Create(objectiveDefinition))
            {
                m_objectives.push_back(std::move(objective));
            }
        }

        log::Info("Drill", "Started '" + definition.name + "'");
        CheckCompletion();
    }

    void DrillRunner::End()
    {
        if (m_drillRoot)
        {
            m_drillRoot->MarkForDestroy();
            m_drillRoot = nullptr;
        }
        m_objectives.clear();
        m_definition = nullptr;
        m_completeFired = false;
    }

    void DrillRunner::Update(float /*deltaTime*/)
    {
        if (m_definition)
        {
            CheckCompletion();
        }
    }

    void DrillRunner::SubmitEvent(const GameEvent& event)
    {
        for (size_t i = 0; i < m_objectives.size(); ++i)
        {
            if (!m_objectives[i]->Handle(event))
            {
                continue;
            }

            log::Info("Objective", m_objectives[i]->Description() + " " +
                std::to_string(m_objectives[i]->CurrentAmount()) + "/" +
                std::to_string(m_objectives[i]->TargetAmount()));

            if (m_objectives[i]->IsComplete())
            {
                log::Info("Objective", "Completed: " + m_objectives[i]->Description());
                onObjectiveCompleted.Emit(static_cast<int>(i));
            }
        }

        onEvent.Emit(event);
        CheckCompletion();
    }

    void DrillRunner::DebugCompleteAllObjectives()
    {
        for (auto& objective : m_objectives)
        {
            objective->ForceComplete();
        }
        CheckCompletion();
    }

    bool DrillRunner::IsComplete() const
    {
        if (m_objectives.empty())
        {
            return false;
        }
        for (const auto& objective : m_objectives)
        {
            if (!objective->IsComplete())
            {
                return false;
            }
        }
        return true;
    }

    int DrillRunner::CompletedObjectiveCount() const
    {
        int count = 0;
        for (const auto& objective : m_objectives)
        {
            if (objective->IsComplete())
            {
                ++count;
            }
        }
        return count;
    }

    void DrillRunner::ConnectEmitters(eng::GameObject* object)
    {
        if (!object)
        {
            return;
        }

        if (auto* target = object->GetComponent<TargetComponent>())
        {
            target->onGameEvent.Connect([this](const GameEvent& event) { SubmitEvent(event); });
        }
        if (auto* knock = object->GetComponent<KnockTargetComponent>())
        {
            knock->onGameEvent.Connect([this](const GameEvent& event) { SubmitEvent(event); });
        }
        if (auto* zone = object->GetComponent<ReachZoneComponent>())
        {
            zone->onGameEvent.Connect([this](const GameEvent& event) { SubmitEvent(event); });
        }

        for (const auto& child : object->GetChildren())
        {
            ConnectEmitters(child.get());
        }
    }

    void DrillRunner::CheckCompletion()
    {
        bool nonFinishComplete = true;
        for (const auto& objective : m_objectives)
        {
            if (objective->Kind() != ObjectiveKind::FinishDrill && !objective->IsComplete())
            {
                nonFinishComplete = false;
                break;
            }
        }

        if (nonFinishComplete)
        {
            for (size_t i = 0; i < m_objectives.size(); ++i)
            {
                auto& objective = m_objectives[i];
                if (objective->Kind() == ObjectiveKind::FinishDrill && !objective->IsComplete())
                {
                    objective->ForceComplete();
                    log::Info("Objective", "Completed: " + objective->Description());
                    onObjectiveCompleted.Emit(static_cast<int>(i));
                }
            }
        }

        if (!m_completeFired && IsComplete())
        {
            m_completeFired = true;
            onAllObjectivesComplete.Emit();
        }
    }
}
