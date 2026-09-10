#include "graphics/Framebuffer.h"

#include <algorithm>

namespace eng
{
    Framebuffer::~Framebuffer()
    {
        Destroy();
    }

    void Framebuffer::Destroy()
    {
        if (m_colorTexture)
        {
            glDeleteTextures(1, &m_colorTexture);
            m_colorTexture = 0;
        }
        if (m_depthStencil)
        {
            glDeleteRenderbuffers(1, &m_depthStencil);
            m_depthStencil = 0;
        }
        if (m_framebuffer)
        {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }
        m_width = 0;
        m_height = 0;
    }

    bool Framebuffer::Resize(int width, int height)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (width == m_width && height == m_height && m_framebuffer != 0)
        {
            return true;
        }

        Destroy();

        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        glGenTextures(1, &m_colorTexture);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

        glGenRenderbuffers(1, &m_depthStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencil);

        const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (!complete)
        {
            Destroy();
            return false;
        }

        m_width = width;
        m_height = height;
        return true;
    }

    void Framebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glViewport(0, 0, m_width, m_height);
    }

    void Framebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
