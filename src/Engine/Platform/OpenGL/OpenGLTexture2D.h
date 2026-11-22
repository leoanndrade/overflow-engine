#pragma once

#include "Engine/Renderer/Texture.h"
#include <glad/gl.h>

namespace Overflow {

    class OpenGLTexture2D : public Texture2D {
    public:
        OpenGLTexture2D(uint32_t width, uint32_t height);
        OpenGLTexture2D(const std::string& path);
        virtual ~OpenGLTexture2D() override;

        virtual uint32_t GetWidth() const override { return m_Width; }
        virtual uint32_t GetHeight() const override { return m_Height; }
        virtual uint32_t GetRendererID() const override { return m_RendererID; }

        virtual void SetData(const void* data, uint32_t size) override;
        virtual void Bind(uint32_t slot = 0) const override;

        virtual bool IsLoaded() const override { return m_IsLoaded; }

        virtual const std::string& GetPath() const override { return m_Path; }

    private:
        std::string m_Path;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_RendererID = 0;
        GLenum m_InternalFormat = 0;
        GLenum m_DataFormat = 0;
        bool m_IsLoaded = false;
    };

}
