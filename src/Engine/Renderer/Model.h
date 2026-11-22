#pragma once

#include "Mesh.h"
#include <string>
#include <vector>
#include <memory>

namespace Overflow {

    class Model {
    public:
        Model() = default;
        Model(const std::string& filepath);
        Model(const std::vector<Mesh>& meshes) : m_Meshes(meshes) {}
        Model(const Mesh& mesh) { m_Meshes.push_back(mesh); }

        bool LoadFromFile(const std::string& filepath);
        void Draw(const std::shared_ptr<Shader>& shader) const;

        inline const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }
        inline std::vector<Mesh>& GetMeshes() { return m_Meshes; }
        inline const std::string& GetFilePath() const { return m_FilePath; }

        void SetTexture(const std::shared_ptr<Texture2D>& texture);

        static std::shared_ptr<Model> Create(const std::string& filepath);
        static std::shared_ptr<Model> CreateFromMesh(const Mesh& mesh) {
            auto model = std::make_shared<Model>();
            model->GetMeshes().push_back(mesh);
            return model;
        }
        static std::shared_ptr<Model> CreateCube(float size = 1.0f);
        static std::shared_ptr<Model> CreateSphere(float radius = 1.0f, uint32_t rings = 24, uint32_t sectors = 48);
        static std::shared_ptr<Model> CreatePlane(float width = 10.0f, float length = 10.0f);

    private:
        bool LoadOBJ(const std::string& filepath);

    private:
        std::vector<Mesh> m_Meshes;
        std::string m_FilePath;
        std::string m_Directory;
    };

}
