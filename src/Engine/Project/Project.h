#pragma once

#include <string>
#include <filesystem>
#include <memory>

namespace Overflow
{
    struct ProjectConfig
    {
        std::string Name = "New Project";
        std::string StartScene = "scenes/demo.scene";
        std::filesystem::path AssetDirectory = "assets";
        std::filesystem::path ScriptDirectory = "assets/scripts";
    };

    class Project
    {
    public:
        static std::shared_ptr<Project> GetActive() { return s_ActiveProject; }
        static void SetActive(std::shared_ptr<Project> project) { s_ActiveProject = project; }

        static std::shared_ptr<Project> NewProject(const std::filesystem::path& projectDir, const std::string& name);
        static std::shared_ptr<Project> LoadProject(const std::filesystem::path& projectFilePath);
        static bool SaveActive(const std::filesystem::path& projectFilePath = "");

        ProjectConfig& GetConfig() { return m_Config; }
        const ProjectConfig& GetConfig() const { return m_Config; }

        const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
        std::filesystem::path GetAssetDirectory() const;
        std::filesystem::path GetScriptDirectory() const;

        bool HasProjectFile() const { return !m_ProjectFilePath.empty(); }
        const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }

    private:
        ProjectConfig m_Config;
        std::filesystem::path m_ProjectDirectory;
        std::filesystem::path m_ProjectFilePath;

        inline static std::shared_ptr<Project> s_ActiveProject = nullptr;
    };
}
