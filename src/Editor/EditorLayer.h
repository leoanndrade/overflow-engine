#pragma once

#include <Engine/Core/Layer.h>
#include <Engine/Core/Timestep.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/PerspectiveCamera.h>
#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Framebuffer.h>
#include <Engine/Renderer/Texture.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>
#include "SceneHierarchyPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentBrowserPanel.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

enum class SceneState {
    Edit = 0,
    Play = 1,
    Pause = 2
};

class EditorLayer : public Overflow::Layer {
public:
    EditorLayer();
    virtual ~EditorLayer() override = default;

    virtual void OnAttach() override;
    virtual void OnDetach() override;
    virtual void OnUpdate(Overflow::Timestep ts) override;
    virtual void OnImGuiRender() override;

private:
    void RenderDockSpace();
    void RenderMenuBar();
    void RenderToolbar();
    void RenderStatusBar();
    void RenderToastNotifications();
    void RenderViewportPanel();
    void RenderGamePanel();
    void RenderMetricsPanel();
    void RenderAboutModal();
    void RenderCreateScriptModal();
    void RenderPreferencesModal();

    void CreateDefaultScene();

    void OnScenePlay();
    void OnScenePause();
    void OnSceneStop();

    void NewScene();
    void OpenScene(const std::string& filepath = "");
    void SaveScene(const std::string& filepath = "");

    void NewProject();
    void OpenProject(const std::string& filepath = "");
    void SaveProject();
    void RenderNewProjectModal();

    void FocusSelectedEntity();
    void DuplicateSelectedEntity();
    void DeleteSelectedEntity();
    void Undo();
    void Redo();
    void ShowToast(const std::string& message, const ImVec4& color = ImVec4(0.35f, 0.70f, 1.0f, 1.0f), float duration = 3.5f);

private:
    struct ToastNotification {
        std::string Message;
        float RemainingSeconds;
        ImVec4 Color;
    };
    std::vector<ToastNotification> m_Toasts;

private:
    // Câmera & Renderização em Framebuffer Multi-Target (MRT)
    Overflow::PerspectiveCamera m_Camera;
    std::shared_ptr<Overflow::Framebuffer> m_Framebuffer;
    std::shared_ptr<Overflow::Framebuffer> m_GameFramebuffer;
    std::shared_ptr<Overflow::Shader> m_Shader;

    // Grafo de Cena e Entidades (ECS)
    std::shared_ptr<Overflow::Scene> m_ActiveScene;
    std::shared_ptr<Overflow::Scene> m_EditorScene;
    SceneState m_SceneState = SceneState::Edit;
    std::string m_CurrentScenePath = "";

    // Painéis do Editor
    Overflow::SceneHierarchyPanel m_SceneHierarchyPanel;
    Overflow::ConsolePanel m_ConsolePanel;
    Overflow::ContentBrowserPanel m_ContentBrowserPanel;

    // Viewport State & Mouse Picking
    glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
    glm::vec2 m_ViewportBounds[2];
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;
    int m_HoveredEntityID = -1;

    // Estado da Câmera estilo Unity / Unreal (Fly, Orbit, Pan)
    bool m_IsCameraFlying = false;
    bool m_IsCameraOrbiting = false;
    bool m_IsCameraPanning = false;
    glm::vec2 m_LastViewportMousePos = { 0.0f, 0.0f };

    // Gizmos 3D (ImGuizmo)
    int m_GizmoType = 0; // 0: TRANSLATE, 1: ROTATE, 2: SCALE, -1: None
    int m_GizmoMode = 0; // 0: LOCAL, 1: WORLD
    bool m_GizmoSnap = false;
    float m_SnapTranslation = 0.5f;
    float m_SnapRotation = 15.0f;
    float m_SnapScale = 0.5f;

    // Sistema de Undo / Redo (Command Pattern - Ctrl+Z e Ctrl+Y)
    struct TransformUndoRecord {
        uint32_t EntityID = 0;
        glm::vec3 OldTranslation{ 0.0f }, NewTranslation{ 0.0f };
        glm::vec3 OldRotation{ 0.0f }, NewRotation{ 0.0f };
        glm::vec3 OldScale{ 1.0f }, NewScale{ 1.0f };
    };
    std::vector<TransformUndoRecord> m_UndoStack;
    std::vector<TransformUndoRecord> m_RedoStack;
    bool m_GizmoWasUsing = false;
    TransformUndoRecord m_CurrentGizmoRecord{};

    bool m_WireframeMode = false;
    bool m_ShowGrid = true;
    float m_Exposure = 1.0f;

    // Gerenciamento de Projetos (.ovproject)
    bool m_ShowNewProjectModal = false;
    char m_NewProjectName[128] = "MyProject";
    char m_NewProjectPath[256] = "projects";

    // Histórico de Performance para o Profiler Sparkline
    float m_FPSHistory[60] = { 0 };
    int m_FPSHistoryIndex = 0;

    // Cor de Limpeza
    glm::vec4 m_ClearColor = { 0.10f, 0.10f, 0.12f, 1.0f };

    // Layout e Docking
    bool m_ResetLayout = false;

    // Visibilidade dos Painéis
    bool m_ShowViewport = true;
    bool m_ShowGameView = true;
    bool m_ShowHierarchy = true;
    bool m_ShowInspector = true;
    bool m_ShowContentBrowser = true;
    bool m_ShowConsole = true;
    bool m_ShowMetrics = true;
    bool m_ShowAboutModal = false;
    bool m_ShowPreferencesModal = false;
    bool m_ShowCreateScriptModal = false;
    char m_NewScriptClassName[128] = "PlayerCombat";

    // Game View State
    glm::vec2 m_GameViewportSize = { 1280.0f, 720.0f };
    bool m_GameViewportFocused = false;
    bool m_GameViewportHovered = false;
    int m_GameAspectRatio = 0; // 0: Free, 1: 16:9, 2: 16:10, 3: 4:3, 4: 21:9, 5: 1:1
    float m_GameScale = 1.0f;
    bool m_MaximizeOnPlay = false;
    bool m_ShowGameStats = true;
    bool m_ShowGameHUD = true;
    bool m_MuteAudio = false;

    // Métricas
    float m_LastFrameTimeMs = 0.0f;
    float m_FPS = 0.0f;
};
