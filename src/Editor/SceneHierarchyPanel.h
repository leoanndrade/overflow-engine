#pragma once

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"

#include <memory>

namespace Overflow {

    class SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Scene>& scene);

        void SetContext(const std::shared_ptr<Scene>& scene);
        void OnImGuiRender();

        Entity GetSelectedEntity() const { return m_SelectionContext; }
        void SetSelectedEntity(Entity entity) { m_SelectionContext = entity; }

        bool IsHierarchyFocused() const { return m_HierarchyFocused; }
        bool CheckEntityDoubleClicked() { bool v = m_EntityDoubleClicked; m_EntityDoubleClicked = false; return v; }

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);

    private:
        std::shared_ptr<Scene> m_Context;
        Entity m_SelectionContext;
        bool m_HierarchyFocused = false;
        bool m_EntityDoubleClicked = false;
    };

}
