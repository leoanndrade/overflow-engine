#include "Framebuffer.h"
#include "Engine/Platform/OpenGL/OpenGLFramebuffer.h"

namespace Overflow {

    std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec) {
        return std::make_shared<OpenGLFramebuffer>(spec);
    }

}
