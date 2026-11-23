#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "Texture.h"
#include "Shader.h"

namespace Overflow
{
    enum class ShaderType
    {
        StandardPBR = 0,
        Unlit = 1,
        BlinnPhong = 2,
        Wireframe = 3
    };

    class Material
    {
    public:
        std::string Name = "New Material";
        std::string FilePath = "";
        ShaderType Type = ShaderType::StandardPBR;

        // Propriedades Básicas e PBR
        glm::vec4 AlbedoColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::shared_ptr<Texture2D> AlbedoTexture;
        std::string AlbedoTexturePath = "";

        float Metallic = 0.0f;   // 0.0 = Dielétrico, 1.0 = Metálico
        float Roughness = 0.4f;  // 0.0 = Espelhado, 1.0 = Fosco
        float AO = 1.0f;         // Oclusão de Ambiente
        float Tiling = 1.0f;     // Escala de repetição de textura

        // Normal Map
        std::shared_ptr<Texture2D> NormalMap;
        std::string NormalMapPath = "";
        bool UseNormalMap = false;

        // Emissão de Luz
        glm::vec3 EmissionColor{ 0.0f };
        float EmissionIntensity = 0.0f;

        // Blinn-Phong específico
        float Shininess = 32.0f;

        Material() = default;
        Material(const std::string& name, ShaderType type = ShaderType::StandardPBR);

        static std::shared_ptr<Material> Create(const std::string& name = "Default Material", ShaderType type = ShaderType::StandardPBR);
        static std::shared_ptr<Material> Load(const std::string& filepath);
        bool Save(const std::string& filepath = "");

        void Bind(const std::shared_ptr<Shader>& shader, const std::shared_ptr<Texture2D>& defaultWhiteTex);

        static const char* GetShaderTypeName(ShaderType type);
        static ShaderType GetShaderTypeFromName(const std::string& name);
    };
}
