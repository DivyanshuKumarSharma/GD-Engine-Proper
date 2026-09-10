#pragma once

#include "course/GameEvent.h"
#include "drills/DrillDefinition.h"
#include "objectives/Objective.h"
#include "util/Signal.h"

#include <memory>
#include <vector>

namespace eng
{
    class Scene;
    class GameObject;
}

namespace game
{
    // Runs one drill: spawns its content under a throw-away "DrillRoot" object,
    // builds Objective instances, wires every target/zone emitter to itself, and
    // routes incoming GameEvents to the objectives. End() destroys the DrillRoot,
    // which tears down all spawned objects, physics bodies and contact listeners
    // together - so restarting can never leak duplicates.
    class DrillRunner
    {
    public:
        explicit DrillRunner(eng::Scene* scene);

        void Begin(const DrillDefinition& definition);
        void End();
        void Update(float deltaTime);

        void SubmitEvent(const GameEvent& event);

        // Test/automation hook: force every objective complete and fire the
        // completion signal, without needing the physical targets to be hit.
        void DebugCompleteAllObjectives();

        bool IsActive() const { return m_definition != nullptr; }
        bool IsComplete() const;
        int CompletedObjectiveCount() const;

        const DrillDefinition* Definition() const { return m_definition; }
        const std::vector<std::unique_ptr<Objective>>& Objectives() const { return m_objectives; }

        Signal<int> onObjectiveCompleted;      // objective index
        Signal<> onAllObjectivesComplete;
        Signal<GameEvent> onEvent;             // every submitted event (HUD feedback)

    private:
        void ConnectEmitters(eng::GameObject* object);
        void CheckCompletion();

        eng::Scene* m_scene = nullptr;
        const DrillDefinition* m_definition = nullptr;
        eng::GameObject* m_drillRoot = nullptr;
        std::vector<std::unique_ptr<Objective>> m_objectives;
        bool m_completeFired = false;
    };
}
