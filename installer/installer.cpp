#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

void CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description, const std::wstring& workingDir)
{
    CoInitialize(NULL);
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
    if (SUCCEEDED(hr))
    {
        psl->SetPath(targetPath.c_str());
        psl->SetDescription(description.c_str());
        psl->SetWorkingDirectory(workingDir.c_str());

        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hr))
        {
            ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize();
}

void RegisterFileAssociation(const std::wstring& exePath)
{
    HKEY hKey;

    // 1. .ovproject -> OverflowEngine.Project
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.ovproject", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        const wchar_t* val = L"OverflowEngine.Project";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // 2. OverflowEngine.Project description
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\OverflowEngine.Project", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        const wchar_t* desc = L"Overflow Engine Project File";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)desc, (DWORD)((wcslen(desc) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // 3. OverflowEngine.Project icon
    std::wstring iconKey = L"Software\\Classes\\OverflowEngine.Project\\DefaultIcon";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, iconKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        std::wstring iconVal = exePath + L",0";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)iconVal.c_str(), (DWORD)((iconVal.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // 4. OverflowEngine.Project shell open command
    std::wstring cmdKey = L"Software\\Classes\\OverflowEngine.Project\\shell\\open\\command";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        std::wstring command = L"\"" + exePath + L"\" \"%1\"";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)((command.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

void RegisterUninstaller(const std::wstring& installDir, const std::wstring& uninstallScript, const std::wstring& exePath)
{
    HKEY hKey;
    std::wstring keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OverflowEngine";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        const wchar_t* name = L"Overflow Engine 3D";
        const wchar_t* ver = L"1.0.0";
        const wchar_t* pub = L"Overflow Engine Team";
        std::wstring cmd = L"\"" + uninstallScript + L"\"";
        std::wstring icon = exePath + L",0";

        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)name, (DWORD)((wcslen(name) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)ver, (DWORD)((wcslen(ver) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)pub, (DWORD)((wcslen(pub) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installDir.c_str(), (DWORD)((installDir.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)icon.c_str(), (DWORD)((icon.length() + 1) * sizeof(wchar_t)));

        RegCloseKey(hKey);
    }
}

bool RunSilentProcess(const std::wstring& command)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 120000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
}

bool ExtractPayload(const fs::path& installDir)
{
    fs::create_directories(installDir);

    // 1. Tenta extrair o payload embutido no recurso ID 101 do executavel
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(101), RT_RCDATA);
    if (hRes)
    {
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (hData)
        {
            DWORD size = SizeofResource(NULL, hRes);
            void* pData = LockResource(hData);
            if (pData && size > 0)
            {
                wchar_t tempPath[MAX_PATH];
                GetTempPathW(MAX_PATH, tempPath);
                fs::path tempZip = fs::path(tempPath) / "OverflowEngine_Payload.zip";

                {
                    std::ofstream out(tempZip, std::ios::binary);
                    if (out.is_open())
                    {
                        out.write(reinterpret_cast<const char*>(pData), size);
                        out.close();
                    }
                }

                if (fs::exists(tempZip))
                {
                    std::wstring cmd = L"tar.exe -xf \"" + tempZip.wstring() + L"\" -C \"" + installDir.wstring() + L"\"";
                    bool ok = RunSilentProcess(cmd);
                    std::error_code ec;
                    fs::remove(tempZip, ec);
                    if (ok && fs::exists(installDir / "bin" / "OverflowEditor.exe"))
                    {
                        return true;
                    }
                }
            }
        }
    }

    // 2. Se nao embutido, verifica se ha 'payload.zip' no mesmo diretorio do instalador
    fs::path localZip = fs::current_path() / "payload.zip";
    if (fs::exists(localZip))
    {
        std::wstring cmd = L"tar.exe -xf \"" + localZip.wstring() + L"\" -C \"" + installDir.wstring() + L"\"";
        if (RunSilentProcess(cmd) && fs::exists(installDir / "bin" / "OverflowEditor.exe"))
        {
            return true;
        }
    }

    // 3. Fallback: copia direta da pasta OverflowEngine se disponivel
    fs::path localDir = fs::current_path() / "OverflowEngine";
    if (fs::exists(localDir))
    {
        fs::copy(localDir, installDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        if (fs::exists(installDir / "bin" / "OverflowEditor.exe"))
        {
            return true;
        }
    }

    return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 1. Mensagem de Boas-Vindas
    int res = MessageBoxW(NULL, 
        L"Bem-vindo ao assistente de instalacao oficial da Overflow Engine 3D v1.0.0!\n\n"
        L"A engine sera instalada no seu perfil de usuario com atalhos e suporte a projetos.\n\n"
        L"Deseja iniciar a instalacao agora?",
        L"Instalador - Overflow Engine 3D", 
        MB_YESNO | MB_ICONQUESTION);

    if (res != IDYES)
    {
        return 0;
    }

    // 2. Determina a pasta de destino (AppData\Local\Programs\Overflow Engine)
    wchar_t localAppData[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData)))
    {
        MessageBoxW(NULL, L"Nao foi possivel acessar o diretorio de programas do usuario.", L"Erro", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path installDir = fs::path(localAppData) / "Programs" / "Overflow Engine";

    // 3. Extrai e instala os arquivos da Engine
    try
    {
        if (!ExtractPayload(installDir))
        {
            MessageBoxW(NULL, 
                L"Nao foi possivel localizar ou extrair os arquivos da engine.\n"
                L"Certifique-se de executar o instalador completo.", 
                L"Erro na Instalacao", 
                MB_OK | MB_ICONERROR);
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::wstring errMsg = L"Erro durante a instalacao: ";
        errMsg += std::wstring(e.what(), e.what() + strlen(e.what()));
        MessageBoxW(NULL, errMsg.c_str(), L"Erro na Instalacao", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path exePath = installDir / "bin" / "OverflowEditor.exe";
    if (!fs::exists(exePath))
    {
        MessageBoxW(NULL, L"O executavel principal nao foi encontrado apos a instalacao.", L"Erro", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 4. Cria Atalho na Area de Trabalho
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath)))
    {
        fs::path shortcut = fs::path(desktopPath) / "Overflow Engine 3D.lnk";
        CreateShortcut(exePath.wstring(), shortcut.wstring(), L"Overflow Engine 3D Editor", (installDir / "bin").wstring());
    }

    // 5. Cria Atalho no Menu Iniciar
    wchar_t programsPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programsPath)))
    {
        fs::path startMenuDir = fs::path(programsPath) / "Overflow Engine";
        fs::create_directories(startMenuDir);
        fs::path shortcut = startMenuDir / "Overflow Engine 3D.lnk";
        CreateShortcut(exePath.wstring(), shortcut.wstring(), L"Overflow Engine 3D Editor", (installDir / "bin").wstring());
    }

    // 6. Associa a extensao .ovproject
    RegisterFileAssociation(exePath.wstring());

    // 7. Cria Script de Desinstalacao
    fs::path uninstallBat = installDir / "Uninstall.bat";
    std::ofstream uninst(uninstallBat);
    if (uninst.is_open())
    {
        uninst << "@echo off\n";
        uninst << "echo Desinstalando Overflow Engine 3D...\n";
        uninst << "taskkill /F /IM OverflowEditor.exe >nul 2>&1\n";
        uninst << "reg delete \"HKCU\\Software\\Classes\\.ovproject\" /f >nul 2>&1\n";
        uninst << "reg delete \"HKCU\\Software\\Classes\\OverflowEngine.Project\" /f >nul 2>&1\n";
        uninst << "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OverflowEngine\" /f >nul 2>&1\n";
        uninst << "del \"%USERPROFILE%\\Desktop\\Overflow Engine 3D.lnk\" >nul 2>&1\n";
        uninst << "rmdir /s /q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Overflow Engine\" >nul 2>&1\n";
        uninst << "start /b \"\" cmd /c timeout /t 1 /nobreak >nul ^& rmdir /s /q \"" << installDir.string() << "\"\n";
        uninst << "echo Overflow Engine removida com sucesso.\n";
        uninst.close();
    }
    RegisterUninstaller(installDir.wstring(), uninstallBat.wstring(), exePath.wstring());

    // 8. Mensagem de Sucesso e opcao de abrir o Editor
    int launchRes = MessageBoxW(NULL, 
        L"A Overflow Engine 3D foi instalada com sucesso no seu computador!\n\n"
        L"Recursos configurados:\n"
        L"- Atalho na Area de Trabalho: 'Overflow Engine 3D'\n"
        L"- Atalho no Menu Iniciar\n"
        L"- Arquivos de projeto (.ovproject) vinculados ao Editor\n"
        L"- Desinstalador registrado no Windows\n\n"
        L"Deseja iniciar a Overflow Engine agora?",
        L"Instalacao Concluida", 
        MB_YESNO | MB_ICONINFORMATION);

    if (launchRes == IDYES)
    {
        ShellExecuteW(NULL, L"open", exePath.wstring().c_str(), NULL, (installDir / "bin").wstring().c_str(), SW_SHOWNORMAL);
    }

    return 0;
}
