#include "Renderer.h"
#include "Engine/Core/Log.h"

namespace Overflow {

    std::unique_ptr<Renderer::SceneData> Renderer::s_SceneData = std::make_unique<Renderer::SceneData>();

    void Renderer::Init() {
        OVERFLOW_CORE_INFO("Inicializando Renderizador da Overflow Engine...");
        RenderCommand::Init();
    }

    void Renderer::Shutdown() {
        OVERFLOW_CORE_INFO("Encerrando Renderizador...");
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height) {
        RenderCommand::SetViewport(0, 0, width, height);
    }

    void Renderer::BeginScene(const PerspectiveCamera& camera) {
        s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    }

    void Renderer::EndScene() {
    }

    void Renderer::Submit(
        const std::shared_ptr<Shader>& shader,
        const std::shared_ptr<VertexArray>& vertexArray,
        const glm::mat4& transform
    ) {
        shader->Bind();
        shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
        shader->SetMat4("u_Transform", transform);

        RenderCommand::DrawIndexed(vertexArray);
    }

    void Renderer::Submit(
        const std::shared_ptr<Shader>& shader,
        const std::shared_ptr<Model>& model,
        const glm::mat4& transform
    ) {
        shader->Bind();
        shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
        shader->SetMat4("u_Transform", transform);

        model->Draw(shader);
    }

}
