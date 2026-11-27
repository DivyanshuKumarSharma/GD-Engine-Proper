#include "scene/components/AudioComponent.h"
#include "scene/GameObject.h"
#include "Engine.h"

#include <algorithm>

namespace eng
{
    void AudioComponent::LoadProperties(const nlohmann::json& json)
    {
        if (json.contains("audio"))
        {
            auto& clipsObject = json["audio"];
            for (auto& clip : clipsObject)
            {
                LoadClip(clip.value("name", "noname"), clip.value("path", ""),
                    clip.value("volume", 1.0f), clip.value("voices", 1));
            }
        }
    }

    bool AudioComponent::LoadClip(const std::string& name, const std::string& path, float volume, int voices)
    {
        Clip clip;
        clip.volume = std::clamp(volume, 0.0f, 1.0f);
        voices = std::clamp(voices, 1, 16);
        for (int i = 0; i < voices; ++i)
        {
            if (auto audio = Audio::Load(path))
            {
                clip.voices.push_back(std::move(audio));
            }
            else
            {
                break; // file missing/undecodable - don't retry it N times
            }
        }
        if (clip.voices.empty())
        {
            return false;
        }
        m_clips[name] = std::move(clip);
        return true;
    }

    bool AudioComponent::HasClip(const std::string& name) const
    {
        return m_clips.find(name) != m_clips.end();
    }

    void AudioComponent::Update(float deltaTime)
    {
        auto pos = m_owner->GetWorldPosition();
        for (auto& clip : m_clips)
        {
            for (auto& voice : clip.second.voices)
            {
                if (voice->IsPlaying())
                {
                    voice->SetPosition(pos);
                }
            }
        }
    }

    void AudioComponent::RegisterAudio(const std::string& name, std::shared_ptr<Audio>& clip)
    {
        if (!clip)
        {
            return;
        }
        Clip entry;
        entry.volume = clip->GetVolume();
        entry.voices.push_back(clip);
        m_clips[name] = std::move(entry);
    }

    void AudioComponent::Play(const std::string& name, bool loop)
    {
        auto it = m_clips.find(name);
        if (it == m_clips.end() || it->second.voices.empty())
        {
            return;
        }
        Clip& clip = it->second;

        // Prefer an idle voice; otherwise steal the next one round-robin (the
        // oldest still-sounding shot), so a burst never silences itself.
        std::shared_ptr<Audio> voice;
        for (std::size_t i = 0; i < clip.voices.size(); ++i)
        {
            auto& candidate = clip.voices[(clip.next + i) % clip.voices.size()];
            if (!candidate->IsPlaying())
            {
                voice = candidate;
                clip.next = (clip.next + i + 1) % clip.voices.size();
                break;
            }
        }
        if (!voice)
        {
            voice = clip.voices[clip.next];
            clip.next = (clip.next + 1) % clip.voices.size();
            voice->Stop();
        }

        const float effects = Engine::GetInstance().GetAudioManager().GetEffectsVolume();
        voice->SetVolume(clip.volume * effects);
        voice->SetPosition(m_owner ? m_owner->GetWorldPosition() : glm::vec3(0.0f));
        voice->Play(loop);
    }

    void AudioComponent::Stop(const std::string& name)
    {
        auto it = m_clips.find(name);
        if (it != m_clips.end())
        {
            for (auto& voice : it->second.voices)
            {
                voice->Stop();
            }
        }
    }

    bool AudioComponent::IsPlaying(const std::string& name)
    {
        auto it = m_clips.find(name);
        if (it != m_clips.end())
        {
            return std::any_of(it->second.voices.begin(), it->second.voices.end(),
                [](const std::shared_ptr<Audio>& v) { return v->IsPlaying(); });
        }
        return false;
    }
}
