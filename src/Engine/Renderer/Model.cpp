#include "Model.h"
#include "Engine/Core/Log.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include <numbers>

namespace std {
    template<> struct hash<Overflow::Vertex> {
        size_t operator()(const Overflow::Vertex& vertex) const noexcept {
            size_t h1 = hash<float>()(vertex.Position.x) ^ (hash<float>()(vertex.Position.y) << 1) ^ (hash<float>()(vertex.Position.z) << 2);
            size_t h2 = hash<float>()(vertex.Normal.x) ^ (hash<float>()(vertex.Normal.y) << 1) ^ (hash<float>()(vertex.Normal.z) << 2);
            size_t h3 = hash<float>()(vertex.TexCoord.x) ^ (hash<float>()(vertex.TexCoord.y) << 1);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

namespace Overflow {

    Model::Model(const std::string& filepath) {
        LoadFromFile(filepath);
    }

    std::shared_ptr<Model> Model::Create(const std::string& filepath) {
        return std::make_shared<Model>(filepath);
    }

    bool Model::LoadFromFile(const std::string& filepath) {
        m_FilePath = filepath;
        std::filesystem::path path(filepath);
        m_Directory = path.parent_path().string();

        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));

        if (ext == ".obj") {
            return LoadOBJ(filepath);
        }

        OVERFLOW_CORE_ERROR("Formato de modelo nao suportado: '{0}'", ext);
        return false;
    }

    void Model::Draw(const std::shared_ptr<Shader>& shader) const {
        for (const auto& mesh : m_Meshes) {
            mesh.Draw(shader);
        }
    }

    void Model::SetTexture(const std::shared_ptr<Texture2D>& texture) {
        for (auto& mesh : m_Meshes) {
            mesh.SetTexture(texture);
        }
    }

    bool Model::LoadOBJ(const std::string& filepath) {
        OVERFLOW_CORE_INFO("Carregando modelo 3D (.obj): '{0}'", filepath);

        std::ifstream file(filepath);
        if (!file.is_open()) {
            OVERFLOW_CORE_ERROR("Nao foi possivel abrir o arquivo .obj: '{0}'", filepath);
            return false;
        }

        std::vector<glm::vec3> temp_positions;
        std::vector<glm::vec2> temp_texcoords;
        std::vector<glm::vec3> temp_normals;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#')
                continue;

            std::stringstream ss(line);
            std::string prefix;
            ss >> prefix;

            if (prefix == "v") {
                glm::vec3 pos;
                ss >> pos.x >> pos.y >> pos.z;
                temp_positions.push_back(pos);
            }
            else if (prefix == "vt") {
                glm::vec2 uv;
                ss >> uv.x >> uv.y;
                temp_texcoords.push_back(uv);
            }
            else if (prefix == "vn") {
                glm::vec3 norm;
                ss >> norm.x >> norm.y >> norm.z;
                temp_normals.push_back(norm);
            }
            else if (prefix == "f") {
                std::vector<std::string> faceTokens;
                std::string token;
                while (ss >> token) {
                    faceTokens.push_back(token);
                }

                if (faceTokens.size() < 3)
                    continue;

                // Triangulação em leque (Fan Triangulation) para polígonos com mais de 3 vértices (ex: quads)
                for (size_t i = 1; i < faceTokens.size() - 1; ++i) {
                    std::string triangleCorners[3] = { faceTokens[0], faceTokens[i], faceTokens[i + 1] };

                    for (int c = 0; c < 3; ++c) {
                        std::stringstream cornerSS(triangleCorners[c]);
                        std::string vStr, vtStr, vnStr;

                        std::getline(cornerSS, vStr, '/');
                        std::getline(cornerSS, vtStr, '/');
                        std::getline(cornerSS, vnStr, '/');

                        Vertex vertex{};
                        vertex.Color = glm::vec4(1.0f);

                        int vIdx = !vStr.empty() ? std::stoi(vStr) : 0;
                        int vtIdx = !vtStr.empty() ? std::stoi(vtStr) : 0;
                        int vnIdx = !vnStr.empty() ? std::stoi(vnStr) : 0;

                        // Ajusta índices OBJ de 1-based para 0-based
                        if (vIdx > 0 && vIdx <= static_cast<int>(temp_positions.size()))
                            vertex.Position = temp_positions[vIdx - 1];
                        else if (vIdx < 0)
                            vertex.Position = temp_positions[temp_positions.size() + vIdx];

                        if (vtIdx > 0 && vtIdx <= static_cast<int>(temp_texcoords.size()))
                            vertex.TexCoord = temp_texcoords[vtIdx - 1];
                        else if (vtIdx < 0)
                            vertex.TexCoord = temp_texcoords[temp_texcoords.size() + vtIdx];
                        else
                            vertex.TexCoord = glm::vec2(0.0f);

                        if (vnIdx > 0 && vnIdx <= static_cast<int>(temp_normals.size()))
                            vertex.Normal = temp_normals[vnIdx - 1];
                        else if (vnIdx < 0)
                            vertex.Normal = temp_normals[temp_normals.size() + vnIdx];
                        else
                            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);

                        // Desduplicação de vértices com tabela hash
                        if (uniqueVertices.count(vertex) == 0) {
                            uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                            vertices.push_back(vertex);
                        }

                        indices.push_back(uniqueVertices[vertex]);
                    }
                }
            }
        }

        // Se o modelo não possui normais calculadas, calcula normais por triângulo
        if (temp_normals.empty() && !indices.empty()) {
            OVERFLOW_CORE_INFO("Modelo sem normais explicitas. Calculando normais de superficie automaticamente...");
            for (size_t i = 0; i < indices.size(); i += 3) {
                Vertex& v0 = vertices[indices[i]];
                Vertex& v1 = vertices[indices[i + 1]];
                Vertex& v2 = vertices[indices[i + 2]];

                glm::vec3 edge1 = v1.Position - v0.Position;
                glm::vec3 edge2 = v2.Position - v0.Position;
                glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

                v0.Normal = normal;
                v1.Normal = normal;
                v2.Normal = normal;
            }
        }

        if (vertices.empty() || indices.empty()) {
            OVERFLOW_CORE_ERROR("Modelo .obj nao continha vertices validos: '{0}'", filepath);
            return false;
        }

        m_Meshes.emplace_back(vertices, indices);
        OVERFLOW_CORE_INFO("Modelo .obj carregado com sucesso: {0} vertices unicos, {1} indices ({2} triangulos)",
            vertices.size(), indices.size(), indices.size() / 3);

        return true;
    }

    std::shared_ptr<Model> Model::CreateCube(float size) {
        auto model = std::make_shared<Model>();
        float half = size * 0.5f;

        std::vector<Vertex> vertices = {
            // Frente
            { { -half, -half,  half }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half, -half,  half }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half,  half }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half,  half,  half }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            // Tras
            { {  half, -half, -half }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half, -half, -half }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half,  half, -half }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half, -half }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            // Cima
            { { -half,  half,  half }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half,  half }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half, -half }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half,  half, -half }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            // Baixo
            { { -half, -half, -half }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half, -half, -half }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half, -half,  half }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half, -half,  half }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            // Direita
            { {  half, -half,  half }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half, -half, -half }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half, -half }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  half,  half,  half }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            // Esquerda
            { { -half, -half, -half }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half, -half,  half }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half,  half,  half }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -half,  half, -half }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
        };

        std::vector<uint32_t> indices = {
            0,  1,  2,  2,  3,  0,
            4,  5,  6,  6,  7,  4,
            8,  9,  10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };

        model->m_Meshes.emplace_back(vertices, indices);
        return model;
    }

    std::shared_ptr<Model> Model::CreateSphere(float radius, uint32_t rings, uint32_t sectors) {
        auto model = std::make_shared<Model>();
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float const R = 1.0f / static_cast<float>(rings - 1);
        float const S = 1.0f / static_cast<float>(sectors - 1);

        constexpr float PI = 3.14159265358979323846f;

        for (uint32_t r = 0; r < rings; ++r) {
            for (uint32_t s = 0; s < sectors; ++s) {
                float const y = sin(-PI * 0.5f + PI * r * R);
                float const x = cos(2.0f * PI * s * S) * sin(PI * r * R);
                float const z = sin(2.0f * PI * s * S) * sin(PI * r * R);

                Vertex v{};
                v.Position = glm::vec3(x, y, z) * radius;
                v.Normal = glm::normalize(glm::vec3(x, y, z));
                v.TexCoord = glm::vec2(s * S, r * R);
                v.Color = glm::vec4(1.0f);

                vertices.push_back(v);
            }
        }

        for (uint32_t r = 0; r < rings - 1; ++r) {
            for (uint32_t s = 0; s < sectors - 1; ++s) {
                uint32_t cur = r * sectors + s;
                uint32_t next = (r + 1) * sectors + s;

                indices.push_back(cur);
                indices.push_back(next);
                indices.push_back(next + 1);

                indices.push_back(cur);
                indices.push_back(next + 1);
                indices.push_back(cur + 1);
            }
        }

        model->m_Meshes.emplace_back(vertices, indices);
        return model;
    }

    std::shared_ptr<Model> Model::CreatePlane(float width, float length) {
        auto model = std::make_shared<Model>();
        float halfW = width * 0.5f;
        float halfL = length * 0.5f;

        std::vector<Vertex> vertices = {
            { { -halfW, 0.0f, -halfL }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  halfW, 0.0f, -halfL }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  halfW, 0.0f,  halfL }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -halfW, 0.0f,  halfL }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0
        };

        model->m_Meshes.emplace_back(vertices, indices);
        return model;
    }

}
