#include "Texture.h"
#include "Engine/Platform/OpenGL/OpenGLTexture2D.h"

namespace Overflow {

    std::shared_ptr<Texture2D> Texture2D::Create(uint32_t width, uint32_t height) {
        return std::make_shared<OpenGLTexture2D>(width, height);
    }

    std::shared_ptr<Texture2D> Texture2D::Create(const std::string& path) {
        return std::make_shared<OpenGLTexture2D>(path);
    }

}
