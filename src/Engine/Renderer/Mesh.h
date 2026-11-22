#pragma once

#include "VertexArray.h"
#include "Texture.h"
#include "Shader.h"

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Overflow {

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
        glm::vec4 Color{ 1.0f };

        bool operator==(const Vertex& other) const {
            return Position == other.Position &&
                   Normal == other.Normal &&
                   TexCoord == other.TexCoord &&
                   Color == other.Color;
        }
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::shared_ptr<Texture2D>& texture);

        void Draw(const std::shared_ptr<Shader>& shader) const;

        inline const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        inline const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
        inline const std::shared_ptr<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        inline const std::shared_ptr<Texture2D>& GetTexture() const { return m_Texture; }
        inline void SetTexture(const std::shared_ptr<Texture2D>& texture) { m_Texture = texture; }

    private:
        void SetupMesh();

    private:
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::shared_ptr<VertexArray> m_VertexArray;
        std::shared_ptr<Texture2D> m_Texture;
    };

}
