@echo off
setlocal
echo Configurando ambiente Visual Studio x64...
call "E:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo Erro ao carregar vcvars64.bat
    exit /b 1
)

cd /d "%~dp0\.."

echo Compilando recursos do instalador...
rc.exe /fo installer\installer.res installer\installer.rc
if errorlevel 1 (
    echo Erro ao compilar installer.rc
    exit /b 1
)

echo Compilando executavel do instalador...
cl.exe /O2 /EHsc /std:c++20 /Fe:dist\OverflowEngine_Setup.exe installer\installer.cpp installer\installer.res user32.lib shell32.lib ole32.lib advapi32.lib
if errorlevel 1 (
    echo Erro ao compilar installer.cpp
    exit /b 1
)

del installer\installer.res 2>nul
del installer.obj 2>nul

echo Instalador gerado com sucesso em dist\OverflowEngine_Setup.exe!
