#include "Mesh.h"
#include "RenderCommand.h"

namespace Overflow {

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
        : m_Vertices(vertices), m_Indices(indices) {
        SetupMesh();
    }

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::shared_ptr<Texture2D>& texture)
        : m_Vertices(vertices), m_Indices(indices), m_Texture(texture) {
        SetupMesh();
    }

    void Mesh::SetupMesh() {
        m_VertexArray = VertexArray::Create();

        auto vertexBuffer = VertexBuffer::Create(m_Vertices.data(), static_cast<uint32_t>(m_Vertices.size() * sizeof(Vertex)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float4, "a_Color" }
        });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        auto indexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
        m_VertexArray->SetIndexBuffer(indexBuffer);
    }

    void Mesh::Draw(const std::shared_ptr<Shader>& /*shader*/) const {
        if (m_Texture) {
            m_Texture->Bind(0);
        }

        RenderCommand::DrawIndexed(m_VertexArray);
    }

}
