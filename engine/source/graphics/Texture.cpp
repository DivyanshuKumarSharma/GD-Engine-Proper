#include "graphics/Texture.h"
#include "Engine.h"
#include "systems/AssetStream.h"
#include "systems/Profiler.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace eng
{
    Texture::Texture(int width, int height, int numChannels, unsigned char* data)
        : m_width(width), m_height(height), m_numChannels(numChannels)
    {
        Init(width, height, numChannels, data);
    }

    Texture::~Texture()
    {
        if (m_textureID > 0)
        {
            glDeleteTextures(1, &m_textureID);
        }
    }

    GLuint Texture::GetID() const
    {
        return m_textureID;
    }

    void Texture::Init(int width, int height, int numChannels, unsigned char* data)
    {
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        GLint internalFormat = GL_RGB;
        GLenum format = GL_RGB;

        if (numChannels == 4)
        {
            internalFormat = GL_RGBA;
            format = GL_RGBA;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    int Texture::GetWidth() const
    {
        return m_width;
    }

    int Texture::GetHeight() const
    {
        return m_height;
    }

    std::shared_ptr<Texture> Texture::Load(const std::string& path)
    {
        int width, height, numChannels;

        auto& fs = Engine::GetInstance().GetFileSystem();
        auto fullPath = fs.GetAssetsFolder() / path;

        if (!std::filesystem::exists(fullPath))
        {
            return nullptr;
        }

        std::shared_ptr<Texture> result;

        unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &numChannels, 0);

        if (data)
        {
            result = std::make_shared<Texture>(width, height, numChannels, data);
            stbi_image_free(data);
        }

        return result;
    }

    std::shared_ptr<Texture> TextureManager::GetOrLoadTexture(const std::string& path)
    {
        const auto key = (Engine::GetInstance().GetFileSystem().GetAssetsFolder() / path).lexically_normal().string();
        auto it = m_textures.find(key);
        if (it != m_textures.end())
        {
            return it->second;
        }

        auto texture = Texture::Load(path);
        m_textures[key] = texture;
        return texture;
    }

    std::shared_ptr<Texture> TextureManager::RequestPreview(const std::string& path)
    {
        auto& engine = Engine::GetInstance();
        const auto absolute = (engine.GetFileSystem().GetAssetsFolder() / path).lexically_normal();
        const auto key = absolute.string();
        // Retire completed requests even when the cursor moved away from the
        // original row; otherwise eight abandoned previews could stall the queue.
        for (auto it = m_previews.begin(); it != m_previews.end();) {
            if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++it; continue; }
            try {
                auto decoded = it->second.get();
                GD_ZONE("Asset preview upload");
                m_textures[it->first] = std::make_shared<Texture>(decoded.width, decoded.height, 4, decoded.rgba.data());
            } catch (const std::exception& e) {
                std::cerr << "Preview " << it->first << ": " << e.what() << '\n'; m_failed.insert(it->first);
            }
            it = m_previews.erase(it);
        }
        if (auto found = m_textures.find(key); found != m_textures.end()) return found->second;
        if (m_failed.count(key)) return nullptr;
        auto pending = m_previews.find(key);
        if (pending == m_previews.end()) {
            if (m_previews.size() >= 8) return nullptr; // bound in-flight decoding memory
            auto& jobs = engine.GetJobs();
            m_previews.emplace(key, jobs.Submit([absolute, &jobs] {
                GD_ZONE("Asset preview read/decode");
                auto read = systems::StreamAsset(jobs, absolute);
                auto bytes = jobs.Wait(read);
                Decoded decoded; int channels = 0;
                if (bytes.empty() || !stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decoded.width, &decoded.height, &channels)
                    || decoded.width <= 0 || decoded.height <= 0 || decoded.width > 4096 || decoded.height > 4096)
                    throw std::runtime_error("preview is invalid or exceeds 4096 pixels per dimension");
                auto* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decoded.width, &decoded.height, &channels, 4);
                if (!pixels) throw std::runtime_error("image decode failed");
                std::unique_ptr<unsigned char, decltype(&stbi_image_free)> owned(pixels, stbi_image_free);
                decoded.rgba.assign(pixels, pixels + static_cast<std::size_t>(decoded.width) * decoded.height * 4);
                return decoded;
            }));
            return nullptr;
        }
        return nullptr;
    }

    void TextureManager::Clear() { m_previews.clear(); m_failed.clear(); m_textures.clear(); }
}
