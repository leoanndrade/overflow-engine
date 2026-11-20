#include "Application.h"
#include "Log.h"

#include <glad/gl.h>

namespace Overflow {

    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name) {
        if (!s_Instance) {
            s_Instance = this;
        }

        Log::Init();
        OVERFLOW_CORE_INFO("Iniciando Overflow Engine Core...");

        m_Window = std::make_unique<Window>(WindowProps(name));
    }

    Application::~Application() {
        OVERFLOW_CORE_INFO("Encerrando Overflow Engine Core...");
    }

    void Application::Close() {
        m_Running = false;
    }

    void Application::Run() {
        OVERFLOW_CORE_INFO("Entrando no Game Loop principal!");

        while (m_Running) {
            if (m_Window->ShouldClose()) {
                m_Running = false;
                break;
            }

            // Limpeza de tela (Buffer de Cor e Profundidade)
            glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Atualização da janela (Swap buffers & poll events)
            m_Window->OnUpdate();
        }

        OVERFLOW_CORE_INFO("Game Loop finalizado.");
    }

}
