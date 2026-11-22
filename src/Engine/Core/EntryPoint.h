#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"

extern Overflow::Application* Overflow::CreateApplication(Overflow::ApplicationCommandLineArgs args);

int main(int argc, char** argv) {
    auto app = Overflow::CreateApplication({ argc, argv });
    app->Run();
    delete app;
    return 0;
}
