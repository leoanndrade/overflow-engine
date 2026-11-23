#include "Material.h"
#include "Engine/Core/Log.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Overflow
{
    Material::Material(const std::string& name, ShaderType type)
        : Name(name), Type(type)
    {
    }

    std::shared_ptr<Material> Material::Create(const std::string& name, ShaderType type)
    {
        return std::make_shared<Material>(name, type);
    }

    const char* Material::GetShaderTypeName(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::StandardPBR: return "Standard PBR";
            case ShaderType::Unlit:       return "Unlit";
            case ShaderType::BlinnPhong:  return "Blinn-Phong";
            case ShaderType::Wireframe:   return "Wireframe";
            default:                      return "Standard PBR";
        }
    }

    ShaderType Material::GetShaderTypeFromName(const std::string& name)
    {
        if (name == "Unlit") return ShaderType::Unlit;
        if (name == "Blinn-Phong") return ShaderType::BlinnPhong;
        if (name == "Wireframe") return ShaderType::Wireframe;
        return ShaderType::StandardPBR;
    }

    bool Material::Save(const std::string& filepath)
    {
        std::string targetPath = filepath.empty() ? FilePath : filepath;
        if (targetPath.empty())
        {
            targetPath = "assets/materials/" + Name + ".ovmat";
        }

        std::filesystem::path p(targetPath);
        if (p.has_parent_path())
        {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream fout(targetPath);
        if (!fout.is_open())
        {
            OVERFLOW_ERROR("Falha ao salvar material: {0}", targetPath);
            return false;
        }

        FilePath = targetPath;

        fout << "# Overflow Engine Material Asset (.ovmat)\n";
        fout << "Material:\n";
        fout << "  Name: \"" << Name << "\"\n";
        fout << "  Shader: \"" << GetShaderTypeName(Type) << "\"\n";
        fout << "  AlbedoColor: [" << AlbedoColor.r << ", " << AlbedoColor.g << ", " << AlbedoColor.b << ", " << AlbedoColor.a << "]\n";
        fout << "  AlbedoTexture: \"" << AlbedoTexturePath << "\"\n";
        fout << "  Metallic: " << Metallic << "\n";
        fout << "  Roughness: " << Roughness << "\n";
        fout << "  AO: " << AO << "\n";
        fout << "  Tiling: " << Tiling << "\n";
        fout << "  NormalMap: \"" << NormalMapPath << "\"\n";
        fout << "  UseNormalMap: " << (UseNormalMap ? 1 : 0) << "\n";
        fout << "  EmissionColor: [" << EmissionColor.r << ", " << EmissionColor.g << ", " << EmissionColor.b << "]\n";
        fout << "  EmissionIntensity: " << EmissionIntensity << "\n";
        fout << "  Shininess: " << Shininess << "\n";

        fout.close();
        OVERFLOW_INFO("Material salvo com sucesso: {0}", targetPath);
        return true;
    }

    std::shared_ptr<Material> Material::Load(const std::string& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            OVERFLOW_WARN("Material nao encontrado: {0}", filepath);
            return nullptr;
        }

        std::ifstream fin(filepath);
        if (!fin.is_open())
        {
            return nullptr;
        }

        auto mat = std::make_shared<Material>();
        mat->FilePath = filepath;
        mat->Name = std::filesystem::path(filepath).stem().string();

        std::string line;
        while (std::getline(fin, line))
        {
            // Remove espaços à esquerda
            size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line[first] == '#') continue;
            std::string trimmed = line.substr(first);

            size_t colon = trimmed.find(':');
            if (colon == std::string::npos) continue;

            std::string key = trimmed.substr(0, colon);
            std::string val = trimmed.substr(colon + 1);

            // Trim val
            size_t vFirst = val.find_first_not_of(" \t\"");
            size_t vLast = val.find_last_not_of(" \t\"");
            std::string cleanVal = "";
            if (vFirst != std::string::npos && vLast != std::string::npos && vLast >= vFirst)
            {
                cleanVal = val.substr(vFirst, vLast - vFirst + 1);
            }

            if (key == "Name")
            {
                mat->Name = cleanVal;
            }
            else if (key == "Shader")
            {
                mat->Type = GetShaderTypeFromName(cleanVal);
            }
            else if (key == "AlbedoColor")
            {
                float r = 1, g = 1, b = 1, a = 1;
                sscanf_s(cleanVal.c_str(), "[%f, %f, %f, %f]", &r, &g, &b, &a);
                mat->AlbedoColor = { r, g, b, a };
            }
            else if (key == "AlbedoTexture")
            {
                mat->AlbedoTexturePath = cleanVal;
                if (!cleanVal.empty() && std::filesystem::exists(cleanVal))
                {
                    mat->AlbedoTexture = Texture2D::Create(cleanVal);
                }
            }
            else if (key == "Metallic")
            {
                mat->Metallic = std::stof(cleanVal);
            }
            else if (key == "Roughness")
            {
                mat->Roughness = std::stof(cleanVal);
            }
            else if (key == "AO")
            {
                mat->AO = std::stof(cleanVal);
            }
            else if (key == "Tiling")
            {
                mat->Tiling = std::stof(cleanVal);
            }
            else if (key == "NormalMap")
            {
                mat->NormalMapPath = cleanVal;
                if (!cleanVal.empty() && std::filesystem::exists(cleanVal))
                {
                    mat->NormalMap = Texture2D::Create(cleanVal);
                }
            }
            else if (key == "UseNormalMap")
            {
                mat->UseNormalMap = (cleanVal == "1" || cleanVal == "true");
            }
            else if (key == "EmissionColor")
            {
                float r = 0, g = 0, b = 0;
                sscanf_s(cleanVal.c_str(), "[%f, %f, %f]", &r, &g, &b);
                mat->EmissionColor = { r, g, b };
            }
            else if (key == "EmissionIntensity")
            {
                mat->EmissionIntensity = std::stof(cleanVal);
            }
            else if (key == "Shininess")
            {
                mat->Shininess = std::stof(cleanVal);
            }
        }

        return mat;
    }

    void Material::Bind(const std::shared_ptr<Shader>& shader, const std::shared_ptr<Texture2D>& defaultWhiteTex)
    {
        if (!shader) return;

        shader->SetFloat4("u_BaseColor", AlbedoColor);
        shader->SetFloat("u_Metallic", Metallic);
        shader->SetFloat("u_Roughness", Roughness);
        shader->SetFloat("u_AO", AO);
        shader->SetFloat("u_Tiling", Tiling);
        shader->SetFloat3("u_EmissionColor", EmissionColor);
        shader->SetFloat("u_EmissionIntensity", EmissionIntensity);
        shader->SetFloat("u_Shininess", Shininess);

        if (AlbedoTexture)
        {
            AlbedoTexture->Bind(0);
        }
        else if (defaultWhiteTex)
        {
            defaultWhiteTex->Bind(0);
        }

        if (UseNormalMap && NormalMap)
        {
            NormalMap->Bind(2);
            shader->SetInt("u_NormalMap", 2);
            shader->SetInt("u_UseNormalMap", 1);
        }
        else
        {
            shader->SetInt("u_UseNormalMap", 0);
        }
    }
}
