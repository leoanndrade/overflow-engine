#include "Buffer.h"
#include "Engine/Platform/OpenGL/OpenGLBuffer.h"
#include "Engine/Core/Log.h"

namespace Overflow {

    std::shared_ptr<VertexBuffer> VertexBuffer::Create(uint32_t size) {
        return std::make_shared<OpenGLVertexBuffer>(size);
    }

    std::shared_ptr<VertexBuffer> VertexBuffer::Create(const void* vertices, uint32_t size) {
        return std::make_shared<OpenGLVertexBuffer>(vertices, size);
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(const uint32_t* indices, uint32_t count) {
        return std::make_shared<OpenGLIndexBuffer>(indices, count);
    }

}
