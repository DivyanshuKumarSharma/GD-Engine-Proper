#include "audio/AudioManager.h"
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>

namespace eng
{
    AudioManager::AudioManager()
    {
        m_engine = std::make_unique<ma_engine>();
    }

    AudioManager::~AudioManager()
    {
        if (m_engine && m_initialized)
        {
            ma_engine_uninit(m_engine.get());
        }
    }

    bool AudioManager::Init()
    {
        auto result = ma_engine_init(nullptr, m_engine.get());
        m_initialized = result == MA_SUCCESS;
        if (m_initialized)
        {
            ma_engine_set_volume(m_engine.get(), m_masterVolume);
        }
        return m_initialized;
    }

    void AudioManager::SetMasterVolume(float volume)
    {
        m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
        if (m_initialized)
        {
            ma_engine_set_volume(m_engine.get(), m_masterVolume);
        }
    }

    void AudioManager::SetEffectsVolume(float volume)
    {
        m_effectsVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    void AudioManager::SetMusicVolume(float volume)
    {
        m_musicVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    ma_engine* AudioManager::GetEngine()
    {
        return m_engine.get();
    }

    void AudioManager::SetListenerPosition(const glm::vec3& pos)
    {
        if (m_engine)
        {
            ma_engine_listener_set_position(m_engine.get(), 0, pos.x, pos.y, pos.z);
        }
    }
}