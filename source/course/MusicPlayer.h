#pragma once

#include <memory>
#include <string>

namespace eng
{
    class Audio;
}

namespace game
{
    // One looping, non-spatial music track at a time. Playing the track that is
    // already loaded never restarts it (no stutter on a redundant call);
    // switching tracks stops the old one first, so two drills' music can never
    // overlap. Volume = track volume x AudioManager's music bus.
    class MusicPlayer
    {
    public:
        ~MusicPlayer();

        // "" stops. Missing/undecodable files are reported once and stay silent.
        void Play(const std::string& assetPath);
        void Stop();
        void Pause();
        void Resume();
        void SetVolume(float volume);   // 0..1 track volume

        const std::string& Current() const { return m_path; }
        bool IsPlaying() const;
        bool IsPaused() const;

    private:
        void ApplyVolume();

        std::shared_ptr<eng::Audio> m_audio;
        std::string m_path;
        std::string m_failedPath;
        float m_volume = 1.0f;
    };
}
