#include "editor/PlayController.h"

#include "editor/EditorLog.h"

#include "Engine.h"
#include "physics/PhysicsManager.h"
#include "scene/Scene.h"

namespace editor
{
    std::shared_ptr<eng::Scene> PlayController::EnterPlay(eng::Scene* editScene, const SnapshotPatch& patch)
    {
        if (!editScene)
        {
            return nullptr;
        }
        m_snapshot = editScene->Serialize();
        m_playTime = 0.0;

        m_playDoc = m_snapshot;
        if (patch)
        {
            patch(m_playDoc);
        }
        auto playScene = eng::Scene::CreateFromDocument(m_playDoc);
        if (!playScene)
        {
            EditorLog::Error("play", "Could not build the play scene from the current course.");
            return nullptr;
        }

        m_state = PlayState::Playing;
        eng::Engine::GetInstance().GetPhysicsManager().SetPaused(false);
        EditorLog::Success("play", "Entered play mode.");
        return playScene;
    }

    std::shared_ptr<eng::Scene> PlayController::ExitPlay()
    {
        m_state = PlayState::Editing;
        m_playTime = 0.0;
        eng::Engine::GetInstance().GetPhysicsManager().SetPaused(true);

        auto editScene = eng::Scene::CreateFromDocument(m_snapshot);
        if (!editScene)
        {
            EditorLog::Error("play", "Failed to restore the authored scene from the snapshot!");
        }
        else
        {
            EditorLog::Success("play", "Returned to the editor; authored scene restored.");
        }
        return editScene;
    }

    std::shared_ptr<eng::Scene> PlayController::Restart()
    {
        if (m_playDoc.is_null())
        {
            return nullptr;
        }
        m_playTime = 0.0;
        m_state = PlayState::Playing;
        eng::Engine::GetInstance().GetPhysicsManager().SetPaused(false);
        EditorLog::Info("play", "Restarted the run.");
        return eng::Scene::CreateFromDocument(m_playDoc);
    }

    void PlayController::SetPaused(bool paused)
    {
        if (m_state == PlayState::Editing)
        {
            return;
        }
        m_state = paused ? PlayState::Paused : PlayState::Playing;
        eng::Engine::GetInstance().GetPhysicsManager().SetPaused(paused);
    }

    void PlayController::TogglePause()
    {
        SetPaused(m_state == PlayState::Playing);
    }

    void PlayController::Tick(float deltaSeconds, eng::Scene* activeScene)
    {
        if (m_state != PlayState::Playing || !activeScene)
        {
            return;
        }
        // Clamp the step so a long stall (dragging the window, a breakpoint)
        // cannot fling physics bodies across the level.
        const float clamped = deltaSeconds > 0.1f ? 0.1f : deltaSeconds;
        activeScene->Update(clamped);
        m_playTime += clamped;
    }
}
