#include "OpenGLTexture2D.h"
#include "Engine/Core/Log.h"

#include <stb_image.h>

namespace Overflow {

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height), m_InternalFormat(GL_RGBA8), m_DataFormat(GL_RGBA) {
        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);
        m_IsLoaded = true;
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_Path(path) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);
        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data) {
            OVERFLOW_CORE_ERROR("Falha ao carregar arquivo de textura: '{0}'", path);
            return;
        }

        m_IsLoaded = true;
        m_Width = static_cast<uint32_t>(width);
        m_Height = static_cast<uint32_t>(height);

        if (channels == 4) {
            m_InternalFormat = GL_RGBA8;
            m_DataFormat = GL_RGBA;
        } else if (channels == 3) {
            m_InternalFormat = GL_RGB8;
            m_DataFormat = GL_RGB;
        } else if (channels == 1) {
            m_InternalFormat = GL_R8;
            m_DataFormat = GL_RED;
        } else {
            m_InternalFormat = GL_RGBA8;
            m_DataFormat = GL_RGBA;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);

        OVERFLOW_CORE_INFO("Textura 2D carregada com sucesso: '{0}' ({1}x{2}, canais: {3})", path, m_Width, m_Height, channels);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::SetData(const void* data, uint32_t size) {
        uint32_t bpp = (m_DataFormat == GL_RGBA) ? 4 : 3;
        if (size != m_Width * m_Height * bpp) {
            OVERFLOW_CORE_WARN("Tamanho de dados para textura incorreto! Esperado: {0}, Recebido: {1}", m_Width * m_Height * bpp, size);
        }

        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
    }

    void OpenGLTexture2D::Bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

}
