#pragma once

#include "Window.h"
#include "LayerStack.h"
#include "Timestep.h"
#include <memory>

namespace Overflow {

    struct ApplicationCommandLineArgs
    {
        int Count = 0;
        char** Args = nullptr;

        const char* operator[](int index) const
        {
            return Args[index];
        }
    };

    class ImGuiLayer;

    class Application {
    public:
        Application(const std::string& name = "Overflow Engine Application", ApplicationCommandLineArgs args = ApplicationCommandLineArgs());
        virtual ~Application();

        void Run();
        void Close();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

        inline Window& GetWindow() { return *m_Window; }
        inline static Application& Get() { return *s_Instance; }
        inline const ApplicationCommandLineArgs& GetCommandLineArgs() const { return m_CommandLineArgs; }

    private:
        ApplicationCommandLineArgs m_CommandLineArgs;
        std::unique_ptr<Window> m_Window;
        ImGuiLayer* m_ImGuiLayer = nullptr;
        LayerStack m_LayerStack;
        bool m_Running = true;
        float m_LastFrameTime = 0.0f;

        static Application* s_Instance;
    };

    // Ponto de entrada implementado pelo cliente (Editor / Jogo)
    Application* CreateApplication(ApplicationCommandLineArgs args = ApplicationCommandLineArgs());

}
