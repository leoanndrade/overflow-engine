#include "RenderCommand.h"

#include <glad/gl.h>

namespace Overflow {

    void RenderCommand::Init() {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        glViewport(x, y, width, height);
    }

    void RenderCommand::SetClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderCommand::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderCommand::DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t count) {
        vertexArray->Bind();
        uint32_t indexCount = count ? count : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }

}
