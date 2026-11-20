#pragma once

#include "Window.h"
#include <memory>

namespace Overflow {

    class Application {
    public:
        Application(const std::string& name = "Overflow Engine Application");
        virtual ~Application();

        void Run();
        void Close();

        inline Window& GetWindow() { return *m_Window; }
        inline static Application& Get() { return *s_Instance; }

    private:
        std::unique_ptr<Window> m_Window;
        bool m_Running = true;

        static Application* s_Instance;
    };

    // Ponto de entrada implementado pelo cliente (Editor / Jogo)
    Application* CreateApplication();

}
