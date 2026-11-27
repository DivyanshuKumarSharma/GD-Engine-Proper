#pragma once

#include "Common.h"
#include <memory>

struct ma_engine;

namespace eng
{
    class AudioManager
    {
    public:
        AudioManager();
        ~AudioManager();

        bool Init();
        bool IsInitialized() const { return m_initialized; }
        ma_engine* GetEngine();

        void SetListenerPosition(const glm::vec3& pos);

        // Mix buses. Master scales the whole engine output; Effects and Music
        // are multipliers applied by AudioComponent (effects) and by whoever
        // plays music (e.g. the course MusicPlayer). All clamped to [0,1].
        void SetMasterVolume(float volume);
        float GetMasterVolume() const { return m_masterVolume; }
        void SetEffectsVolume(float volume);
        float GetEffectsVolume() const { return m_effectsVolume; }
        void SetMusicVolume(float volume);
        float GetMusicVolume() const { return m_musicVolume; }

    private:
        std::unique_ptr<ma_engine> m_engine;
        bool m_initialized = false;
        float m_masterVolume = 1.0f;
        float m_effectsVolume = 1.0f;
        float m_musicVolume = 1.0f;
    };
}