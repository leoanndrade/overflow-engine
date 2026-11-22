#include "ScriptManager.h"
#include "Engine/Core/Log.h"
#include "Engine/Scene/ScriptRegistry.h"

#include <windows.h>
#include <fstream>
#include <cstdlib>

namespace Overflow {

    extern void ForceLinkUserScripts();

    void* ScriptManager::s_ScriptModuleHandle = nullptr;
    std::string ScriptManager::s_DLLStatus = "Native C++ Embedded";

    void ScriptManager::Init() {
        ForceLinkUserScripts();
        OVERFLOW_INFO("ScriptManager inicializado.");
        ReloadScriptsDLL();
    }

    void ScriptManager::Shutdown() {
        if (s_ScriptModuleHandle) {
            FreeLibrary(static_cast<HMODULE>(s_ScriptModuleHandle));
            s_ScriptModuleHandle = nullptr;
        }
    }

    bool ScriptManager::CreateScript(const std::string& scriptName, std::string& outFilePath) {
        if (scriptName.empty()) return false;

        // Limpeza e validação de nome de classe C++
        std::string validName = "";
        for (char c : scriptName) {
            if (isalnum(c) || c == '_') validName += c;
        }
        if (validName.empty() || isdigit(validName[0])) {
            validName = "My" + validName;
        }

        std::filesystem::path dir("src/Engine/Scene/Scripts");
        std::filesystem::create_directories(dir);

        std::filesystem::path filePath = dir / (validName + ".h");
        outFilePath = filePath.string();

        if (std::filesystem::exists(filePath)) {
            OVERFLOW_WARN("Script ja existe em '{0}'. Pulando criacao para evitar sobrescrita.", outFilePath);
            return false;
        }

        std::string templateCode = "#pragma once\n\n"
                                   "#include \"Engine/Scene/ScriptableEntity.h\"\n"
                                   "#include \"Engine/Scene/Component.h\"\n"
                                   "#include \"Engine/Core/Input.h\"\n"
                                   "#include \"Engine/Core/KeyCodes.h\"\n"
                                   "#include \"Engine/Core/Log.h\"\n\n"
                                   "namespace Overflow\n{\n"
                                   "    // Script OBehaviour nativo da Overflow Engine\n"
                                   "    class " + validName + " : public OBehaviour\n    {\n"
                                   "    public:\n"
                                   "        float Speed = 5.0f;\n\n"
                                   "    protected:\n"
                                   "        void OnInit() override\n        {\n"
                                   "            OVERFLOW_INFO(\"" + validName + " inicializado!\");\n"
                                   "        }\n\n"
                                   "        void OnEnable() override\n        {\n"
                                   "            OVERFLOW_INFO(\"" + validName + " habilitado!\");\n"
                                   "        }\n\n"
                                   "        void OnTick(Timestep ts) override\n        {\n"
                                   "            if (!HasComponent<TransformComponent>())\n            {\n"
                                   "                return;\n            }\n"
                                   "            auto& tc = GetComponent<TransformComponent>();\n"
                                   "            // Logica de comportamento e fisica no OnTick:\n"
                                   "            // tc.Rotation.y += Speed * 10.0f * ts.GetSeconds();\n"
                                   "        }\n\n"
                                   "        void OnDisable() override\n        {\n"
                                   "            OVERFLOW_INFO(\"" + validName + " desabilitado!\");\n"
                                   "        }\n\n"
                                   "        void OnDestroy() override\n        {\n"
                                   "            OVERFLOW_INFO(\"" + validName + " destruido!\");\n"
                                   "        }\n"
                                   "    };\n"
                                   "}\n";

        std::ofstream fout(filePath);
        if (!fout.is_open()) {
            OVERFLOW_ERROR("Nao foi possivel criar arquivo de script em: '{0}'", outFilePath);
            return false;
        }
        fout << templateCode;
        fout.close();

        // Registra automaticamente no master header UserScripts.h
        std::filesystem::path masterHeader = "src/Engine/Scene/UserScripts.h";
        if (std::filesystem::exists(masterHeader)) {
            std::string includeLine = "Scripts/" + validName + ".h";
            std::ifstream fin(masterHeader);
            std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
            fin.close();

            if (content.find(includeLine) == std::string::npos) {
                std::ofstream appendMaster(masterHeader, std::ios::app);
                appendMaster << "\n#if __has_include(\"" << includeLine << "\")\n#include \"" << includeLine << "\"\n#endif\n";
                appendMaster.close();
            }
        }

        // Registra centralizadamente no UserScriptsRegistry.cpp (script 100% puro para o usuario)
        std::filesystem::path registryCpp = "src/Engine/Scene/UserScriptsRegistry.cpp";
        if (std::filesystem::exists(registryCpp)) {
            std::ifstream fin(registryCpp);
            std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
            fin.close();

            std::string regCall = "ScriptRegistry::Register<" + validName + ">(\"" + validName + "\", \"Custom OBehaviour\");";
            if (content.find(regCall) == std::string::npos) {
                size_t pos = content.rfind("OVERFLOW_INFO");
                if (pos != std::string::npos) {
                    content.insert(pos, "        " + regCall + "\n\n        ");
                    std::ofstream foutReg(registryCpp);
                    foutReg << content;
                    foutReg.close();
                }
            }
        }

        OVERFLOW_INFO("Script C++ criado com sucesso: '{0}'", outFilePath);
        return true;
    }

    bool ScriptManager::OpenInRider() {
        OVERFLOW_INFO("Tentando abrir projeto no JetBrains Rider...");

        // 1. Tenta abrir diretamente com o comando rider se estiver no PATH
        HINSTANCE res = ShellExecuteA(NULL, "open", "rider.cmd", "build\\OverflowEngine.sln", NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        res = ShellExecuteA(NULL, "open", "rider64.exe", "build\\OverflowEngine.sln", NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        res = ShellExecuteA(NULL, "open", "rider.cmd", ".", NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        res = ShellExecuteA(NULL, "open", "rider64.exe", ".", NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        // 2. Procura em caminhos padrao do JetBrains Rider e Toolbox
        char localAppData[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH)) {
            std::string toolboxPath = std::string(localAppData) + "\\Programs\\Rider\\bin\\rider64.exe";
            if (std::filesystem::exists(toolboxPath)) {
                res = ShellExecuteA(NULL, "open", toolboxPath.c_str(), "build\\OverflowEngine.sln", NULL, SW_SHOWNORMAL);
                if ((intptr_t)res > 32) return true;
            }
        }

        // 3. Tenta abrir o arquivo .sln diretamente (associação padrão do Windows para o Rider / Visual Studio)
        res = ShellExecuteA(NULL, "open", "build\\OverflowEngine.sln", NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        // 4. Fallback: abre o diretório do projeto no Explorer
        ShellExecuteA(NULL, "open", "explorer.exe", ".", NULL, SW_SHOWNORMAL);
        OVERFLOW_INFO("Diretorio do projeto aberto no Explorer para carregar no Rider.");
        return true;
    }

    bool ScriptManager::ReloadScriptsDLL() {
        std::string dllPath = "build/bin/Release/GameScripts.dll";
        if (!std::filesystem::exists(dllPath)) {
            dllPath = "GameScripts.dll";
        }

        if (!std::filesystem::exists(dllPath)) {
            s_DLLStatus = "Embedded C++ (No external DLL found)";
            return false;
        }

        // Libera a biblioteca anterior se carregada
        if (s_ScriptModuleHandle) {
            FreeLibrary(static_cast<HMODULE>(s_ScriptModuleHandle));
            s_ScriptModuleHandle = nullptr;
        }

        // Faz cópia temporária para evitar travamento do arquivo pelo compilador (Live Coding)
        std::string tempDll = "build/bin/Release/GameScripts_temp.dll";
        try {
            std::filesystem::copy_file(dllPath, tempDll, std::filesystem::copy_options::overwrite_existing);
        } catch (...) {
            tempDll = dllPath;
        }

        HMODULE mod = LoadLibraryA(tempDll.c_str());
        if (!mod) {
            OVERFLOW_WARN("Nao foi possivel carregar DLL de scripts: '{0}'", tempDll);
            s_DLLStatus = "Error loading DLL";
            return false;
        }

        s_ScriptModuleHandle = mod;

        typedef void (*InitScriptsFn)();
        InitScriptsFn initFunc = (InitScriptsFn)GetProcAddress(mod, "InitScripts");
        if (initFunc) {
            initFunc();
            OVERFLOW_INFO("Scripts externos da DLL carregados com sucesso!");
            s_DLLStatus = "Loaded (Hot-Reload Active)";
            return true;
        }

        s_DLLStatus = "Loaded (No InitScripts found)";
        return true;
    }

    bool ScriptManager::IsDLLLoaded() {
        return s_ScriptModuleHandle != nullptr;
    }

    const std::string& ScriptManager::GetDLLStatus() {
        return s_DLLStatus;
    }

    bool ScriptManager::TriggerCompile() {
        OVERFLOW_INFO("Iniciando compilacao dos scripts via CMake Release...");
        std::string cmd = "cmake --build build --config Release";
        int res = std::system(cmd.c_str());
        if (res == 0) {
            ReloadScriptsDLL();
            return true;
        }
        return false;
    }

}
