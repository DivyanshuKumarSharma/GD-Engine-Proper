#pragma once

#include <GL/glew.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <future>
#include <vector>
#include <unordered_set>

namespace eng
{
    class Texture
    {
    public:
        Texture(int width, int height, int numChannels, unsigned char* data);
        ~Texture();
        GLuint GetID() const;
        void Init(int width, int height, int numChannels, unsigned char* data);

        int GetWidth() const;
        int GetHeight() const;

        static std::shared_ptr<Texture> Load(const std::string& path);

    private:
        int m_width = 0;
        int m_height = 0;
        int m_numChannels = 0;
        GLuint m_textureID = 0;
    };

    class TextureManager
    {
    public:
        std::shared_ptr<Texture> GetOrLoadTexture(const std::string& path);
        // Pollable background file read/decode; GL upload occurs on this caller.
        std::shared_ptr<Texture> RequestPreview(const std::string& path);
        void Clear();

    private:
        std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
        struct Decoded { int width = 0, height = 0; std::vector<unsigned char> rgba; };
        std::unordered_map<std::string, std::future<Decoded>> m_previews;
        std::unordered_set<std::string> m_failed;
    };
}
