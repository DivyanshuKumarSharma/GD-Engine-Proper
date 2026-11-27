#pragma once
#include "Common.h"
#include <memory>
#include <string>
#include <vector>

struct ma_sound;
struct ma_decoder;

namespace eng
{
    class Audio
    {
    public:
        ~Audio();
        void SetPosition(const glm::vec3& position);
        void Play(bool loop = false);
        void Stop();               // stops and rewinds
        void Pause();              // stops without rewinding; Resume() continues
        void Resume();
        bool IsPlaying() const;
        bool IsPaused() const { return m_paused; }
        void SetVolume(float volume);
        float GetVolume() const;
        // 3D attenuation/panning on (default, for world sounds) or off (music, UI).
        void SetSpatialized(bool spatialized);

        static std::shared_ptr<Audio> Load(const std::string& path);

    private:
        std::unique_ptr<ma_sound> m_sound;
        std::unique_ptr<ma_decoder> m_decoder;
        std::vector<char> m_buffer;
        bool m_paused = false;
        bool m_decoderReady = false;
        bool m_soundReady = false;
    };
}