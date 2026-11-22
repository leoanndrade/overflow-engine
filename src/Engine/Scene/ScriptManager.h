#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace Overflow {

    class ScriptManager {
    public:
        static void Init();
        static void Shutdown();

        // 1. Criador de Scripts C++ Nativo (estilo Unity / Unreal)
        static bool CreateScript(const std::string& scriptName, std::string& outFilePath);

        // 2. Abertura do Projeto no JetBrains Rider
        static bool OpenInRider();

        // 3. Hot-Reloading de DLL de Gameplay
        static bool ReloadScriptsDLL();
        static bool IsDLLLoaded();
        static const std::string& GetDLLStatus();

        // 4. Compilação dos scripts via MSVC/CMake
        static bool TriggerCompile();

    private:
        static void* s_ScriptModuleHandle;
        static std::string s_DLLStatus;
    };

}
