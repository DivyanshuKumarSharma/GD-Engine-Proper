#include "audio/Audio.h"
#include "Engine.h"
#include <miniaudio.h>

namespace eng
{
    Audio::~Audio()
    {
        // Only tear down what Load() actually initialized - a clip whose decoder
        // rejected the file must not uninit a never-initialized ma_sound.
        if (m_sound && m_soundReady)
        {
            ma_sound_uninit(m_sound.get());
        }
        if (m_decoder && m_decoderReady)
        {
            ma_decoder_uninit(m_decoder.get());
        }
    }

    void Audio::SetPosition(const glm::vec3& position)
    {
        if (m_sound)
        {
            ma_sound_set_position(m_sound.get(), position.x, position.y, position.z);
        }
    }

    void Audio::Play(bool loop)
    {
        if (m_sound)
        {
            m_paused = false;
            ma_sound_set_looping(m_sound.get(), loop ? MA_TRUE : MA_FALSE);
            ma_sound_start(m_sound.get());
        }
    }

    void Audio::Stop()
    {
        if (m_sound)
        {
            m_paused = false;
            ma_sound_stop(m_sound.get());
            ma_sound_seek_to_pcm_frame(m_sound.get(), 0);
        }
    }

    void Audio::Pause()
    {
        if (m_sound && ma_sound_is_playing(m_sound.get()))
        {
            ma_sound_stop(m_sound.get());
            m_paused = true;
        }
    }

    void Audio::Resume()
    {
        if (m_sound && m_paused)
        {
            m_paused = false;
            ma_sound_start(m_sound.get());
        }
    }

    void Audio::SetSpatialized(bool spatialized)
    {
        if (m_sound)
        {
            ma_sound_set_spatialization_enabled(m_sound.get(), spatialized ? MA_TRUE : MA_FALSE);
        }
    }

    bool Audio::IsPlaying() const
    {
        if (m_sound)
        {
            return ma_sound_is_playing(m_sound.get());
        }
        return false;
    }

    void Audio::SetVolume(float volume)
    {
        if (m_sound)
        {
            ma_sound_set_volume(m_sound.get(), std::clamp(volume, 0.0f, 1.0f));
        }
    }

    float Audio::GetVolume() const
    {
        if (m_sound)
        {
            return ma_sound_get_volume(m_sound.get());
        }
        return 0.0f;
    }

    std::shared_ptr<Audio> Audio::Load(const std::string& path)
    {
        auto& audioManager = Engine::GetInstance().GetAudioManager();
        if (!audioManager.IsInitialized())
        {
            return nullptr; // no audio device - callers treat a null clip as "silent"
        }
        auto buffer = Engine::GetInstance().GetFileSystem().LoadAssetFile(path);
        if (buffer.empty())
        {
            return nullptr;
        }
        auto engine = audioManager.GetEngine();

        auto audio = std::make_shared<Audio>();
        audio->m_sound = std::make_unique<ma_sound>();
        audio->m_buffer = buffer;
        audio->m_decoder = std::make_unique<ma_decoder>();
        auto result = ma_decoder_init_memory(audio->m_buffer.data(), audio->m_buffer.size(), 
            nullptr, audio->m_decoder.get());
        if (result != MA_SUCCESS)
        {
            return nullptr;
        }
        audio->m_decoderReady = true;
        result = ma_sound_init_from_data_source(
            engine,
            audio->m_decoder.get(),
            0,
            NULL,
            audio->m_sound.get()
        );
        if (result != MA_SUCCESS)
        {
            return nullptr;
        }
        audio->m_soundReady = true;

        ma_sound_set_spatialization_enabled(audio->m_sound.get(), MA_TRUE);
        return audio;
    }
}