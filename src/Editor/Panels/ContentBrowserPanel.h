#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <functional>

namespace Overflow {

    enum class ContentBrowserViewMode {
        Grid = 0,
        List = 1
    };

    class ContentBrowserPanel {
    public:
        ContentBrowserPanel();
        ~ContentBrowserPanel() = default;

        void OnImGuiRender(bool* p_open = nullptr);

        const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
        void SetCurrentDirectory(const std::filesystem::path& path) { m_CurrentDirectory = path; }

        const std::filesystem::path& GetBaseDirectory() const { return m_BaseDirectory; }
        void SetBaseDirectory(const std::filesystem::path& path) {
            m_BaseDirectory = path;
            m_CurrentDirectory = path;
        }

        void SetOpenSceneCallback(const std::function<void(const std::string&)>& callback) {
            m_OpenSceneCallback = callback;
        }

        void SetInstantiateArchetypeCallback(const std::function<void(const std::string&)>& callback) {
            m_InstantiateArchetypeCallback = callback;
        }

    private:
        void DrawFolderTree(const std::filesystem::path& currentPath);
        void RenderHeaderToolbar();
        void RenderGridView(const std::vector<std::filesystem::directory_entry>& entries);
        void RenderListView(const std::vector<std::filesystem::directory_entry>& entries);
        void RenderContextMenu();
        void RenderModals();
        void HandleItemDoubleClick(const std::filesystem::path& path, bool isDirectory);

        void CreateScriptFile(const std::string& name);
        void CreateShaderFile(const std::string& name);
        void CreateMaterialFile(const std::string& name);
        void CreateArchetypeFile(const std::string& name);

    private:
        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;
        float m_ThumbnailSize = 78.0f;
        ContentBrowserViewMode m_ViewMode = ContentBrowserViewMode::Grid;
        char m_SearchFilter[128] = "";

        // Callbacks de integração
        std::function<void(const std::string&)> m_OpenSceneCallback = nullptr;
        std::function<void(const std::string&)> m_InstantiateArchetypeCallback = nullptr;

        // Modais de Criação de Assets
        bool m_ShowCreateScriptModal = false;
        char m_NewScriptName[64] = "MyBehavior";

        bool m_ShowCreateShaderModal = false;
        char m_NewShaderName[64] = "CustomShader";

        bool m_ShowCreateMaterialModal = false;
        char m_NewMaterialName[64] = "NewMaterial";

        bool m_ShowCreateArchetypeModal = false;
        char m_NewArchetypeName[64] = "NewActorBlueprint";

        bool m_ShowNewFolderModal = false;
        char m_NewFolderName[64] = "NewFolder";

        // Item selecionado atualmente no browser
        std::filesystem::path m_SelectedPath = "";
    };

}
