#include "VertexArray.h"
#include "Engine/Platform/OpenGL/OpenGLVertexArray.h"

namespace Overflow {

    std::shared_ptr<VertexArray> VertexArray::Create() {
        return std::make_shared<OpenGLVertexArray>();
    }

}
