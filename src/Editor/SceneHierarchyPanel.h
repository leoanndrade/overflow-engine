#pragma once

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture.h"

#include <memory>
#include <filesystem>
#include <string>

namespace Overflow {

    class SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Scene>& scene);

        void SetContext(const std::shared_ptr<Scene>& scene);
        void OnImGuiRender();

        Entity GetSelectedEntity() const { return m_SelectionContext; }
        void SetSelectedEntity(Entity entity);

        const std::filesystem::path& GetSelectedAsset() const { return m_SelectedAsset; }
        void SetSelectedAsset(const std::filesystem::path& assetPath);
        void ClearSelection();

        bool IsHierarchyFocused() const { return m_HierarchyFocused; }
        bool CheckEntityDoubleClicked() { bool v = m_EntityDoubleClicked; m_EntityDoubleClicked = false; return v; }

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);
        void DrawAssetInspector(const std::filesystem::path& assetPath);

    private:
        std::shared_ptr<Scene> m_Context;
        Entity m_SelectionContext;
        std::filesystem::path m_SelectedAsset = "";

        // Cache para inspeção de assets
        std::shared_ptr<Material> m_InspectedMaterial;
        std::string m_InspectedMaterialPath = "";
        std::shared_ptr<Texture2D> m_InspectedTexture;
        std::string m_InspectedTexturePath = "";
        std::string m_CachedFileContent = "";
        std::string m_CachedFilePath = "";

        bool m_HierarchyFocused = false;
        bool m_EntityDoubleClicked = false;
    };

}
