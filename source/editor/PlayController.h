#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>

namespace eng
{
    class Scene;
}

namespace editor
{
    enum class PlayState
    {
        Editing,
        Playing,
        Paused
    };

    // Owns the edit <-> play transition. Play always runs on a fresh Scene built
    // from a JSON snapshot of the authored scene, so stopping restores the
    // authored state exactly (object positions, deleted objects, timers, ...)
    // simply by discarding the play scene and rebuilding from the snapshot.
    class PlayController
    {
    public:
        PlayState State() const { return m_state; }
        bool IsPlaying() const { return m_state != PlayState::Editing; }

        // Snapshots `editScene` and returns a new, independent scene to simulate.
        // The caller swaps its scene pointer to this and hands it to the engine.
        // `patch` (optional) may mutate the JSON snapshot before it is built -
        // the editor uses it to bake authored movement / weapon settings into
        // the player without touching the authored scene.
        using SnapshotPatch = std::function<void(nlohmann::json&)>;
        std::shared_ptr<eng::Scene> EnterPlay(eng::Scene* editScene, const SnapshotPatch& patch = {});

        // Rebuilds the authored scene from the snapshot taken by EnterPlay.
        std::shared_ptr<eng::Scene> ExitPlay();

        // Rebuilds the play scene from the snapshot (restart the run).
        std::shared_ptr<eng::Scene> Restart();

        void SetPaused(bool paused);
        void TogglePause();

        // Advances the play scene when not paused.
        void Tick(float deltaSeconds, eng::Scene* activeScene);

        double PlayTimeSeconds() const { return m_playTime; }
        const nlohmann::json& Snapshot() const { return m_snapshot; }

    private:
        PlayState m_state = PlayState::Editing;
        nlohmann::json m_snapshot;   // authored state, restored on Stop
        nlohmann::json m_playDoc;    // snapshot + editor patches, used for Restart
        double m_playTime = 0.0;
    };
}
