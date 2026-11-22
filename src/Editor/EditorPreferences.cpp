#include "EditorPreferences.h"
#include "Engine/Core/Log.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <fstream>
#include <sstream>

namespace Overflow {

    CodeEditorChoice EditorPreferences::s_PreferredIDE = CodeEditorChoice::JetBrainsRider;
    std::string EditorPreferences::s_CustomIDEPath = "";
    std::string EditorPreferences::s_PreferencesFilePath = "overflow_preferences.ini";

    void EditorPreferences::Init() {
        if (!std::filesystem::exists(s_PreferencesFilePath)) {
            Save();
            return;
        }

        std::ifstream file(s_PreferencesFilePath);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);

            // Trim
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));

            if (key == "PreferredIDE") {
                try {
                    int choice = std::stoi(val);
                    if (choice >= 0 && choice <= 4) {
                        s_PreferredIDE = static_cast<CodeEditorChoice>(choice);
                    }
                } catch (...) {}
            } else if (key == "CustomIDEPath") {
                s_CustomIDEPath = val;
            }
        }
        file.close();
        OVERFLOW_INFO("Preferencias da Engine carregadas (IDE preferida: {0})", GetIDEName(s_PreferredIDE));
    }

    void EditorPreferences::Save() {
        std::ofstream file(s_PreferencesFilePath);
        if (!file.is_open()) return;

        file << "# Overflow Engine Preferences\n";
        file << "PreferredIDE=" << static_cast<int>(s_PreferredIDE) << "\n";
        file << "CustomIDEPath=" << s_CustomIDEPath << "\n";
        file.close();
        OVERFLOW_INFO("Preferencias salvas com sucesso em '{0}'.", s_PreferencesFilePath);
    }

    CodeEditorChoice EditorPreferences::GetPreferredIDE() {
        return s_PreferredIDE;
    }

    void EditorPreferences::SetPreferredIDE(CodeEditorChoice ide) {
        s_PreferredIDE = ide;
        Save();
    }

    const std::string& EditorPreferences::GetCustomIDEPath() {
        return s_CustomIDEPath;
    }

    void EditorPreferences::SetCustomIDEPath(const std::string& path) {
        s_CustomIDEPath = path;
        Save();
    }

    const char* EditorPreferences::GetIDEName(CodeEditorChoice ide) {
        switch (ide) {
            case CodeEditorChoice::JetBrainsRider: return "JetBrains Rider";
            case CodeEditorChoice::VisualStudio:  return "Visual Studio";
            case CodeEditorChoice::VSCode:        return "Visual Studio Code";
            case CodeEditorChoice::SystemDefault: return "System Default";
            case CodeEditorChoice::Custom:        return "Custom Executable";
            default:                              return "Unknown";
        }
    }

    bool EditorPreferences::OpenFileInIDE(const std::filesystem::path& filepath) {
        std::string absPath = std::filesystem::absolute(filepath).string();
        OVERFLOW_INFO("Abrindo arquivo '{0}' na IDE preferida: {1}", absPath, GetIDEName(s_PreferredIDE));

        if (!s_CustomIDEPath.empty() && s_PreferredIDE == CodeEditorChoice::Custom) {
#ifdef _WIN32
            std::string args = "\"" + absPath + "\"";
            HINSTANCE res = ShellExecuteA(NULL, "open", s_CustomIDEPath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
            if ((intptr_t)res > 32) return true;
#endif
        }

        switch (s_PreferredIDE) {
            case CodeEditorChoice::JetBrainsRider:
                return OpenInRider(absPath);
            case CodeEditorChoice::VisualStudio:
                return OpenInVisualStudio(absPath);
            case CodeEditorChoice::VSCode:
                return OpenInVSCode(absPath);
            case CodeEditorChoice::SystemDefault:
            default:
#ifdef _WIN32
                ShellExecuteA(NULL, "open", absPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                return true;
#else
                return false;
#endif
        }
    }

    bool EditorPreferences::OpenInRider(const std::filesystem::path& filepath) {
#ifdef _WIN32
        std::string pathStr = filepath.string();
        std::string args = "\"" + pathStr + "\"";

        // 1. Tenta comando direto do rider
        HINSTANCE res = ShellExecuteA(NULL, "open", "rider64.exe", args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        res = ShellExecuteA(NULL, "open", "rider.cmd", args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        // 2. Procura em caminhos padrão de instalação do JetBrains Rider / Toolbox
        char localAppData[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH)) {
            std::string tb = std::string(localAppData) + "\\Programs\\Rider\\bin\\rider64.exe";
            if (std::filesystem::exists(tb)) {
                res = ShellExecuteA(NULL, "open", tb.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
                if ((intptr_t)res > 32) return true;
            }
        }

        // 3. Fallback: abre com o padrão do sistema
        ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return true;
#else
        return false;
#endif
    }

    bool EditorPreferences::OpenInVisualStudio(const std::filesystem::path& filepath) {
#ifdef _WIN32
        std::string pathStr = filepath.string();
        std::string args = "/Edit \"" + pathStr + "\"";

        // 1. Verifica instalação do Visual Studio 2022
        const char* candidatePaths[] = {
            "E:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\devenv.exe",
            "E:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe"
        };

        for (const char* p : candidatePaths) {
            if (std::filesystem::exists(p)) {
                HINSTANCE res = ShellExecuteA(NULL, "open", p, args.c_str(), NULL, SW_SHOWNORMAL);
                if ((intptr_t)res > 32) return true;
            }
        }

        HINSTANCE res = ShellExecuteA(NULL, "open", "devenv.exe", args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        // Fallback
        ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return true;
#else
        return false;
#endif
    }

    bool EditorPreferences::OpenInVSCode(const std::filesystem::path& filepath) {
#ifdef _WIN32
        std::string pathStr = filepath.string();
        std::string args = "\"" + pathStr + "\"";

        HINSTANCE res = ShellExecuteA(NULL, "open", "code.cmd", args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        res = ShellExecuteA(NULL, "open", "code.exe", args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)res > 32) return true;

        char localAppData[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH)) {
            std::string codePath = std::string(localAppData) + "\\Programs\\Microsoft VS Code\\Code.exe";
            if (std::filesystem::exists(codePath)) {
                res = ShellExecuteA(NULL, "open", codePath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
                if ((intptr_t)res > 32) return true;
            }
        }

        // Fallback
        ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return true;
#else
        return false;
#endif
    }

}
