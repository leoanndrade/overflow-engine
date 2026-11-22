#pragma once

#include <string>
#include <filesystem>

namespace Overflow {

    enum class CodeEditorChoice {
        JetBrainsRider = 0,
        VisualStudio = 1,
        VSCode = 2,
        SystemDefault = 3,
        Custom = 4
    };

    class EditorPreferences {
    public:
        static void Init();
        static void Save();

        static CodeEditorChoice GetPreferredIDE();
        static void SetPreferredIDE(CodeEditorChoice ide);

        static const std::string& GetCustomIDEPath();
        static void SetCustomIDEPath(const std::string& path);

        static bool OpenFileInIDE(const std::filesystem::path& filepath);

        static const char* GetIDEName(CodeEditorChoice ide);

    private:
        static CodeEditorChoice s_PreferredIDE;
        static std::string s_CustomIDEPath;
        static std::string s_PreferencesFilePath;

        static bool OpenInRider(const std::filesystem::path& filepath);
        static bool OpenInVisualStudio(const std::filesystem::path& filepath);
        static bool OpenInVSCode(const std::filesystem::path& filepath);
    };

}
