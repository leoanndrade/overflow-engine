#include "Application.h"
#include "Log.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/ImGui/ImGuiLayer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

namespace Overflow {

    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name, ApplicationCommandLineArgs args)
        : m_CommandLineArgs(args) {
        if (!s_Instance) {
            s_Instance = this;
        }

        Log::Init();
        OVERFLOW_CORE_INFO("Iniciando Overflow Engine Core...");

        m_Window = std::make_unique<Window>(WindowProps(name));

        // Inicializa o subsistema de renderização
        Renderer::Init();

        // Inicializa o layer do ImGui como overlay do topo
        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);
    }

    Application::~Application() {
        Renderer::Shutdown();
        OVERFLOW_CORE_INFO("Encerrando Overflow Engine Core...");
    }

    void Application::Close() {
        m_Running = false;
    }

    void Application::PushLayer(Layer* layer) {
        m_LayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(Layer* overlay) {
        m_LayerStack.PushOverlay(overlay);
    }

    void Application::Run() {
        OVERFLOW_CORE_INFO("Entrando no Game Loop principal!");

        while (m_Running) {
            if (m_Window->ShouldClose()) {
                m_Running = false;
                break;
            }

            float time = static_cast<float>(glfwGetTime());
            Timestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;

            // 1. Atualização da lógica de cada camada (ex: renderiza no Framebuffer offscreen)
            for (Layer* layer : m_LayerStack) {
                layer->OnUpdate(timestep);
            }

            // Limpa o backbuffer da janela principal para renderização da GUI do Editor
            glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // 2. Renderização das interfaces ImGui
            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack) {
                layer->OnImGuiRender();
            }
            m_ImGuiLayer->End();

            // 3. Atualização da janela (Swap buffers & poll events)
            m_Window->OnUpdate();
        }

        OVERFLOW_CORE_INFO("Game Loop finalizado.");
    }

}
