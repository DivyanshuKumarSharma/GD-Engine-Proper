#pragma once
#include "scene/Component.h"
#include "audio/Audio.h"

#include <vector>

namespace eng
{
    // Named sound clips on an object. A clip may own several "voices" (separate
    // decoders of the same file) so rapid re-triggers - automatic gunfire - can
    // overlap instead of cutting the previous shot off:
    //   { "name": "shoot", "path": "audio/shoot.wav", "volume": 0.8, "voices": 4 }
    // Every play is scaled by AudioManager's effects bus.
    class AudioComponent : public Component
    {
        COMPONENT(AudioComponent)
    public:
        void LoadProperties(const nlohmann::json& json) override;
        void Update(float deltaTime) override;

        void RegisterAudio(const std::string& name, std::shared_ptr<Audio>& clip);
        // Loads `path` (with `voices` overlapping instances). Returns false if
        // the file could not be decoded; the clip is then simply silent.
        bool LoadClip(const std::string& name, const std::string& path, float volume = 1.0f, int voices = 1);
        bool HasClip(const std::string& name) const;

        void Play(const std::string& name, bool loop = false);
        void Stop(const std::string& name);
        bool IsPlaying(const std::string& name);

    private:
        struct Clip
        {
            std::vector<std::shared_ptr<Audio>> voices;
            float volume = 1.0f;
            std::size_t next = 0;
        };
        std::unordered_map<std::string, Clip> m_clips;
    };
}
