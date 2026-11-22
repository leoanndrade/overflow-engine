#include "OpenGLFramebuffer.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>

namespace Overflow {

    static const uint32_t s_MaxFramebufferSize = 8192;

    namespace Utils {
        static bool IsDepthFormat(FramebufferTextureFormat format) {
            switch (format) {
                case FramebufferTextureFormat::DEPTH24STENCIL8: return true;
                default: return false;
            }
        }
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
        : m_Specification(spec) {
        for (auto format : m_Specification.Attachments.Attachments) {
            if (!Utils::IsDepthFormat(format.TextureFormat))
                m_ColorAttachmentSpecifications.emplace_back(format);
            else
                m_DepthAttachmentSpecification = format;
        }

        if (m_ColorAttachmentSpecifications.empty() && m_DepthAttachmentSpecification.TextureFormat == FramebufferTextureFormat::None) {
            m_ColorAttachmentSpecifications.emplace_back(FramebufferTextureFormat::RGBA8);
            m_DepthAttachmentSpecification = FramebufferTextureFormat::DEPTH24STENCIL8;
        }

        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer() {
        glDeleteFramebuffers(1, &m_RendererID);
        glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
        glDeleteTextures(1, &m_DepthAttachment);
    }

    void OpenGLFramebuffer::Invalidate() {
        if (m_RendererID) {
            glDeleteFramebuffers(1, &m_RendererID);
            glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
            glDeleteTextures(1, &m_DepthAttachment);
            m_ColorAttachments.clear();
            m_DepthAttachment = 0;
        }

        glGenFramebuffers(1, &m_RendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

        // 1. Cria Anexos de Cor
        if (!m_ColorAttachmentSpecifications.empty()) {
            m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
            glGenTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());

            for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
                glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[i]);

                switch (m_ColorAttachmentSpecifications[i].TextureFormat) {
                    case FramebufferTextureFormat::RGBA8: {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Specification.Width, m_Specification.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i), GL_TEXTURE_2D, m_ColorAttachments[i], 0);
                        break;
                    }
                    case FramebufferTextureFormat::RED_INTEGER: {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, m_Specification.Width, m_Specification.Height, 0, GL_RED_INTEGER, GL_INT, nullptr);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i), GL_TEXTURE_2D, m_ColorAttachments[i], 0);
                        break;
                    }
                    default: break;
                }
            }
        }

        // 2. Cria Anexo de Profundidade/Stencil
        if (m_DepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None) {
            glGenTextures(1, &m_DepthAttachment);
            glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
        }

        // 3. Configura buffers de desenho
        if (m_ColorAttachments.size() > 1) {
            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
        } else if (m_ColorAttachments.empty()) {
            glDrawBuffer(GL_NONE);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            OVERFLOW_CORE_ERROR("Erro: Framebuffer nao esta completo!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glViewport(0, 0, m_Specification.Width, m_Specification.Height);
    }

    void OpenGLFramebuffer::Unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize) {
            OVERFLOW_CORE_WARN("Tentativa de redimensionar Framebuffer para tamanho invalido: {0}x{1}", width, height);
            return;
        }

        m_Specification.Width = width;
        m_Specification.Height = height;

        Invalidate();
    }

    int OpenGLFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) {
        if (attachmentIndex >= m_ColorAttachments.size())
            return -1;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
        int pixelData = -1;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        return pixelData;
    }

    void OpenGLFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value) {
        if (attachmentIndex >= m_ColorAttachments.size())
            return;

        auto& spec = m_ColorAttachmentSpecifications[attachmentIndex];
        if (spec.TextureFormat == FramebufferTextureFormat::RED_INTEGER) {
            glClearTexImage(m_ColorAttachments[attachmentIndex], 0, GL_RED_INTEGER, GL_INT, &value);
        }
    }

}
