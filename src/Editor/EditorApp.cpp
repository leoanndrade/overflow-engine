#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>
#include "EditorLayer.h"

class EditorApp : public Overflow::Application {
public:
    EditorApp(Overflow::ApplicationCommandLineArgs args) : Overflow::Application("Overflow Engine 3D - Editor", args) {
        OVERFLOW_INFO("Overflow Editor inicializado com sucesso!");
        PushLayer(new EditorLayer());
    }

    ~EditorApp() override {
        OVERFLOW_INFO("Overflow Editor finalizado.");
    }
};

Overflow::Application* Overflow::CreateApplication(Overflow::ApplicationCommandLineArgs args) {
    return new EditorApp(args);
}
