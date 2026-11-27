#include "course/MusicPlayer.h"

#include "util/Log.h"

#include <eng.h>

#include <algorithm>

namespace game
{
    MusicPlayer::~MusicPlayer()
    {
        Stop();
    }

    void MusicPlayer::Play(const std::string& assetPath)
    {
        if (assetPath.empty())
        {
            Stop();
            return;
        }
        if (assetPath == m_path && m_audio)
        {
            if (m_audio->IsPaused())
            {
                m_audio->Resume();
            }
            else if (!m_audio->IsPlaying())
            {
                m_audio->Play(true);
            }
            return;
        }

        Stop();
        if (assetPath == m_failedPath)
        {
            return; // already reported; don't re-read a bad file every drill
        }
        m_audio = eng::Audio::Load(assetPath);
        if (!m_audio)
        {
            m_failedPath = assetPath;
            log::Warn("Music", "could not play '" + assetPath + "' (missing, unsupported format, or no audio device)");
            return;
        }
        m_path = assetPath;
        m_audio->SetSpatialized(false);
        ApplyVolume();
        m_audio->Play(true);
    }

    void MusicPlayer::Stop()
    {
        if (m_audio)
        {
            m_audio->Stop();
        }
        m_audio.reset();
        m_path.clear();
    }

    void MusicPlayer::Pause()
    {
        if (m_audio)
        {
            m_audio->Pause();
        }
    }

    void MusicPlayer::Resume()
    {
        if (m_audio)
        {
            m_audio->Resume();
        }
    }

    void MusicPlayer::SetVolume(float volume)
    {
        m_volume = std::clamp(volume, 0.0f, 1.0f);
        ApplyVolume();
    }

    void MusicPlayer::ApplyVolume()
    {
        if (m_audio)
        {
            const float bus = eng::Engine::GetInstance().GetAudioManager().GetMusicVolume();
            m_audio->SetVolume(m_volume * bus);
        }
    }

    bool MusicPlayer::IsPlaying() const
    {
        return m_audio && m_audio->IsPlaying();
    }

    bool MusicPlayer::IsPaused() const
    {
        return m_audio && m_audio->IsPaused();
    }
}
