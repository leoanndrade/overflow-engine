#include "OpenGLVertexArray.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>

namespace Overflow {

    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type) {
        switch (type) {
            case ShaderDataType::Float:    return GL_FLOAT;
            case ShaderDataType::Float2:   return GL_FLOAT;
            case ShaderDataType::Float3:   return GL_FLOAT;
            case ShaderDataType::Float4:   return GL_FLOAT;
            case ShaderDataType::Mat3:     return GL_FLOAT;
            case ShaderDataType::Mat4:     return GL_FLOAT;
            case ShaderDataType::Int:      return GL_INT;
            case ShaderDataType::Int2:     return GL_INT;
            case ShaderDataType::Int3:     return GL_INT;
            case ShaderDataType::Int4:     return GL_INT;
            case ShaderDataType::Bool:     return GL_BOOL;
            default: break;
        }
        OVERFLOW_CORE_ERROR("Tipo ShaderDataType desconhecido!");
        return 0;
    }

    OpenGLVertexArray::OpenGLVertexArray() {
        glGenVertexArrays(1, &m_RendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray() {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const {
        glBindVertexArray(m_RendererID);
    }

    void OpenGLVertexArray::Unbind() const {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
        if (vertexBuffer->GetLayout().GetElements().empty()) {
            OVERFLOW_CORE_ERROR("VertexBuffer nao possui layout definido ao adicionar ao VertexArray!");
            return;
        }

        glBindVertexArray(m_RendererID);
        vertexBuffer->Bind();

        const auto& layout = vertexBuffer->GetLayout();
        for (const auto& element : layout) {
            switch (element.Type) {
                case ShaderDataType::Float:
                case ShaderDataType::Float2:
                case ShaderDataType::Float3:
                case ShaderDataType::Float4: {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(
                        m_VertexBufferIndex,
                        element.GetComponentCount(),
                        ShaderDataTypeToOpenGLBaseType(element.Type),
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        (const void*)element.Offset
                    );
                    m_VertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Int:
                case ShaderDataType::Int2:
                case ShaderDataType::Int3:
                case ShaderDataType::Int4:
                case ShaderDataType::Bool: {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribIPointer(
                        m_VertexBufferIndex,
                        element.GetComponentCount(),
                        ShaderDataTypeToOpenGLBaseType(element.Type),
                        layout.GetStride(),
                        (const void*)element.Offset
                    );
                    m_VertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Mat3:
                case ShaderDataType::Mat4: {
                    uint32_t count = element.GetComponentCount();
                    for (uint32_t i = 0; i < count; i++) {
                        glEnableVertexAttribArray(m_VertexBufferIndex);
                        glVertexAttribPointer(
                            m_VertexBufferIndex,
                            count,
                            ShaderDataTypeToOpenGLBaseType(element.Type),
                            element.Normalized ? GL_TRUE : GL_FALSE,
                            layout.GetStride(),
                            (const void*)(element.Offset + sizeof(float) * count * i)
                        );
                        glVertexAttribDivisor(m_VertexBufferIndex, 1);
                        m_VertexBufferIndex++;
                    }
                    break;
                }
                default:
                    OVERFLOW_CORE_ERROR("ShaderDataType desconhecido ao configurar atributos do VertexArray!");
                    break;
            }
        }

        m_VertexBuffers.push_back(vertexBuffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
        glBindVertexArray(m_RendererID);
        indexBuffer->Bind();

        m_IndexBuffer = indexBuffer;
    }

}
