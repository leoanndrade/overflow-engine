#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>

class EditorApp : public Overflow::Application {
public:
    EditorApp() : Overflow::Application("Overflow Engine 3D - Editor") {
        OVERFLOW_INFO("Overflow Editor inicializado com sucesso!");
    }

    ~EditorApp() override {
        OVERFLOW_INFO("Overflow Editor finalizado.");
    }
};

Overflow::Application* Overflow::CreateApplication() {
    return new EditorApp();
}
