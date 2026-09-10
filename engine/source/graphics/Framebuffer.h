#pragma once

#include <GL/glew.h>

namespace eng
{
    // A minimal offscreen render target (RGBA8 colour texture + packed depth24/
    // stencil8 renderbuffer). The editor renders the scene into one of these so
    // the 3D view can live inside a dockable ImGui panel; the runtime never
    // touches it and keeps drawing straight to the window backbuffer.
    class Framebuffer
    {
    public:
        Framebuffer() = default;
        ~Framebuffer();

        Framebuffer(const Framebuffer&) = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        // (Re)allocates storage when the size actually changes. Sizes are clamped
        // to at least 1x1. Returns false only if the GL objects could not be
        // created.
        bool Resize(int width, int height);

        // Binds this framebuffer and sets the GL viewport to cover it.
        void Bind();
        // Restores the default framebuffer (0). Does not touch the viewport.
        static void Unbind();

        GLuint GetColorTexture() const { return m_colorTexture; }
        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }
        bool IsValid() const { return m_framebuffer != 0; }

    private:
        void Destroy();

        GLuint m_framebuffer = 0;
        GLuint m_colorTexture = 0;
        GLuint m_depthStencil = 0;
        int m_width = 0;
        int m_height = 0;
    };
}
