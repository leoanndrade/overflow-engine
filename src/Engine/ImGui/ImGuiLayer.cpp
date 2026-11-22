#include "ImGuiLayer.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <filesystem>

namespace Overflow {

    ImGuiLayer::ImGuiLayer()
        : Layer("ImGuiLayer") {
    }

    void ImGuiLayer::OnAttach() {
        OVERFLOW_CORE_INFO("Inicializando Dear ImGui com suporte a Docking...");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // Tipografia moderna de alta resolução estilo Unreal Engine 5 / Blender 4
        if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
            ImFontConfig fontConfig;
            fontConfig.OversampleH = 3;
            fontConfig.OversampleV = 3;
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &fontConfig);
            io.FontDefault = io.Fonts->Fonts.back();
        }

        SetDarkThemeColors();

        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        OVERFLOW_CORE_INFO("Dear ImGui Docking inicializado com sucesso!");
    }

    void ImGuiLayer::OnDetach() {
        OVERFLOW_CORE_INFO("Encerrando Dear ImGui...");
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::Begin() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::End() {
        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::Get();
        io.DisplaySize = ImVec2(static_cast<float>(app.GetWindow().GetWidth()), static_cast<float>(app.GetWindow().GetHeight()));

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void ImGuiLayer::SetDarkThemeColors() {
        auto& style = ImGui::GetStyle();
        auto& colors = style.Colors;

        // Estilo geométrico e espaçamento moderno estilo Unreal Engine 5 / Blender 4
        style.WindowRounding    = 0.0f; // Janelas dockadas DEVEM ter borda 0 para se encaixar perfeitamente!
        style.ChildRounding     = 0.0f;
        style.FrameRounding     = 3.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding      = 3.0f;
        style.TabRounding       = 4.0f;

        style.WindowBorderSize  = 0.0f;
        style.ChildBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.0f;
        style.FrameBorderSize   = 0.0f;
        style.TabBorderSize     = 0.0f;

        style.WindowPadding     = ImVec2(8.0f, 8.0f);
        style.FramePadding      = ImVec2(6.0f, 4.0f);
        style.ItemSpacing       = ImVec2(6.0f, 5.0f);
        style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

        // Paleta de Cores Escuras Profissionais (Tema Unreal 5 Slate / Obsidian)
        colors[ImGuiCol_WindowBg]             = ImVec4{ 0.11f, 0.115f, 0.13f, 1.0f };
        colors[ImGuiCol_ChildBg]              = ImVec4{ 0.11f, 0.115f, 0.13f, 1.0f };
        colors[ImGuiCol_PopupBg]              = ImVec4{ 0.13f, 0.135f, 0.155f, 0.98f };

        // Headers
        colors[ImGuiCol_Header]               = ImVec4{ 0.18f, 0.20f, 0.24f, 1.0f };
        colors[ImGuiCol_HeaderHovered]        = ImVec4{ 0.24f, 0.27f, 0.32f, 1.0f };
        colors[ImGuiCol_HeaderActive]         = ImVec4{ 0.28f, 0.32f, 0.38f, 1.0f };

        // Botões com acento elegante
        colors[ImGuiCol_Button]               = ImVec4{ 0.17f, 0.18f, 0.21f, 1.0f };
        colors[ImGuiCol_ButtonHovered]        = ImVec4{ 0.24f, 0.38f, 0.62f, 1.0f };
        colors[ImGuiCol_ButtonActive]         = ImVec4{ 0.18f, 0.30f, 0.52f, 1.0f };

        // Frames / Inputs
        colors[ImGuiCol_FrameBg]              = ImVec4{ 0.08f, 0.085f, 0.095f, 1.0f };
        colors[ImGuiCol_FrameBgHovered]       = ImVec4{ 0.15f, 0.16f, 0.19f, 1.0f };
        colors[ImGuiCol_FrameBgActive]        = ImVec4{ 0.20f, 0.22f, 0.26f, 1.0f };

        // Tabs (ImGui 1.90+)
        colors[ImGuiCol_Tab]                  = ImVec4{ 0.13f, 0.135f, 0.155f, 1.0f };
        colors[ImGuiCol_TabHovered]           = ImVec4{ 0.24f, 0.40f, 0.68f, 1.0f };
        colors[ImGuiCol_TabSelected]          = ImVec4{ 0.17f, 0.28f, 0.48f, 1.0f };
        colors[ImGuiCol_TabDimmed]            = ImVec4{ 0.11f, 0.115f, 0.13f, 1.0f };
        colors[ImGuiCol_TabDimmedSelected]    = ImVec4{ 0.14f, 0.17f, 0.22f, 1.0f };

        // Títulos de Janela
        colors[ImGuiCol_TitleBg]              = ImVec4{ 0.09f, 0.095f, 0.105f, 1.0f };
        colors[ImGuiCol_TitleBgActive]        = ImVec4{ 0.11f, 0.12f, 0.14f, 1.0f };
        colors[ImGuiCol_TitleBgCollapsed]     = ImVec4{ 0.09f, 0.095f, 0.105f, 1.0f };

        // Menu Bar
        colors[ImGuiCol_MenuBarBg]            = ImVec4{ 0.09f, 0.095f, 0.105f, 1.0f };

        // Barras de Rolagem e Sliders
        colors[ImGuiCol_ScrollbarBg]          = ImVec4{ 0.08f, 0.085f, 0.095f, 0.5f };
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4{ 0.22f, 0.24f, 0.28f, 1.0f };
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.30f, 0.34f, 0.40f, 1.0f };
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4{ 0.38f, 0.44f, 0.52f, 1.0f };
        colors[ImGuiCol_CheckMark]            = ImVec4{ 0.35f, 0.65f, 1.00f, 1.0f };
        colors[ImGuiCol_SliderGrab]           = ImVec4{ 0.32f, 0.55f, 0.90f, 1.0f };
        colors[ImGuiCol_SliderGrabActive]     = ImVec4{ 0.42f, 0.68f, 1.00f, 1.0f };

        // Bordas e Separadores
        colors[ImGuiCol_Border]               = ImVec4{ 0.16f, 0.17f, 0.20f, 1.0f };
        colors[ImGuiCol_BorderShadow]         = ImVec4{ 0.00f, 0.00f, 0.00f, 0.0f };
        colors[ImGuiCol_Separator]            = ImVec4{ 0.16f, 0.17f, 0.20f, 1.0f };
        colors[ImGuiCol_SeparatorHovered]     = ImVec4{ 0.30f, 0.48f, 0.78f, 1.0f };
        colors[ImGuiCol_SeparatorActive]      = ImVec4{ 0.40f, 0.60f, 0.90f, 1.0f };

        // Docking Preview
        colors[ImGuiCol_DockingPreview]       = ImVec4{ 0.25f, 0.45f, 0.85f, 0.6f };
        colors[ImGuiCol_DockingEmptyBg]       = ImVec4{ 0.08f, 0.085f, 0.095f, 1.0f };
    }

}
