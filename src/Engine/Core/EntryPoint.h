#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"

extern Overflow::Application* Overflow::CreateApplication();

int main(int argc, char** argv) {
    auto app = Overflow::CreateApplication();
    app->Run();
    delete app;
    return 0;
}
