#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>

class EditorApp : public Engine::Application {
public:
    EditorApp() : Engine::Application("Custom 3D Engine - Editor (Estudo)") {
        ENGINE_INFO("Editor inicializado com sucesso!");
    }

    ~EditorApp() override {
        ENGINE_INFO("Editor destruido.");
    }
};

Engine::Application* Engine::CreateApplication() {
    return new EditorApp();
}
