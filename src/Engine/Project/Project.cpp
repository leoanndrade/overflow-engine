#include "Project.h"
#include "Engine/Core/Log.h"

#include <fstream>
#include <sstream>

namespace Overflow
{
    std::filesystem::path Project::GetAssetDirectory() const
    {
        if (m_ProjectDirectory.empty())
        {
            return m_Config.AssetDirectory;
        }
        return m_ProjectDirectory / m_Config.AssetDirectory;
    }

    std::filesystem::path Project::GetScriptDirectory() const
    {
        if (m_ProjectDirectory.empty())
        {
            return m_Config.ScriptDirectory;
        }
        return m_ProjectDirectory / m_Config.ScriptDirectory;
    }

    std::shared_ptr<Project> Project::NewProject(const std::filesystem::path& projectDir, const std::string& name)
    {
        auto project = std::make_shared<Project>();
        project->m_Config.Name = name.empty() ? "MyProject" : name;
        project->m_ProjectDirectory = projectDir;

        // Cria a árvore de pastas padrão do projeto
        std::filesystem::create_directories(projectDir / "assets" / "scenes");
        std::filesystem::create_directories(projectDir / "assets" / "scripts");
        std::filesystem::create_directories(projectDir / "assets" / "textures");
        std::filesystem::create_directories(projectDir / "assets" / "models");
        std::filesystem::create_directories(projectDir / "assets" / "materials");
        std::filesystem::create_directories(projectDir / "assets" / "archetypes");

        std::filesystem::path projectFilePath = projectDir / (project->m_Config.Name + ".ovproject");
        project->m_ProjectFilePath = projectFilePath;

        SaveActive(projectFilePath);
        s_ActiveProject = project;

        OVERFLOW_INFO("Novo projeto Overflow criado em: '{0}'", projectFilePath.string());
        return project;
    }

    std::shared_ptr<Project> Project::LoadProject(const std::filesystem::path& projectFilePath)
    {
        if (!std::filesystem::exists(projectFilePath))
        {
            OVERFLOW_ERROR("Arquivo de projeto nao encontrado: '{0}'", projectFilePath.string());
            return nullptr;
        }

        auto project = std::make_shared<Project>();
        project->m_ProjectFilePath = projectFilePath;
        project->m_ProjectDirectory = projectFilePath.parent_path();

        std::ifstream file(projectFilePath);
        if (!file.is_open())
        {
            OVERFLOW_ERROR("Nao foi possivel abrir o arquivo de projeto: '{0}'", projectFilePath.string());
            return nullptr;
        }

        std::string line;
        while (std::getline(file, line))
        {
            auto colonPos = line.find(':');
            if (colonPos != std::string::npos)
            {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n\""));
                value.erase(value.find_last_not_of(" \t\r\n\"") + 1);

                if (key == "Name") project->m_Config.Name = value;
                else if (key == "StartScene") project->m_Config.StartScene = value;
                else if (key == "AssetDirectory") project->m_Config.AssetDirectory = value;
                else if (key == "ScriptDirectory") project->m_Config.ScriptDirectory = value;
            }
        }
        file.close();

        s_ActiveProject = project;
        OVERFLOW_INFO("Projeto Overflow carregado: '{0}' (Diretorio: '{1}')", 
                      project->m_Config.Name, project->m_ProjectDirectory.string());
        return project;
    }

    bool Project::SaveActive(const std::filesystem::path& projectFilePath)
    {
        if (!s_ActiveProject)
        {
            return false;
        }

        std::filesystem::path path = projectFilePath.empty() ? s_ActiveProject->m_ProjectFilePath : projectFilePath;
        if (path.empty())
        {
            path = s_ActiveProject->m_ProjectDirectory / (s_ActiveProject->m_Config.Name + ".ovproject");
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            OVERFLOW_ERROR("Nao foi possivel salvar arquivo de projeto: '{0}'", path.string());
            return false;
        }

        file << "# Overflow Engine Project File\n";
        file << "Name: \"" << s_ActiveProject->m_Config.Name << "\"\n";
        file << "EngineVersion: \"1.0.0\"\n";
        file << "StartScene: \"" << s_ActiveProject->m_Config.StartScene << "\"\n";
        file << "AssetDirectory: \"" << s_ActiveProject->m_Config.AssetDirectory.string() << "\"\n";
        file << "ScriptDirectory: \"" << s_ActiveProject->m_Config.ScriptDirectory.string() << "\"\n";

        file.close();
        s_ActiveProject->m_ProjectFilePath = path;
        OVERFLOW_INFO("Projeto Overflow salvo em: '{0}'", path.string());
        return true;
    }
}
