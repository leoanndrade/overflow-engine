@echo off
setlocal
echo ========================================================
echo  Overflow Engine 3D - Build ^& Package Standalone Setup
echo ========================================================

cd /d "%~dp0\.."

echo 1. Compilando o Editor em Release...
"E:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
if errorlevel 1 (
    echo Erro ao compilar OverflowEditor!
    exit /b 1
)

echo 2. Empacotando arquivos de distribuicao e SDK...
powershell -ExecutionPolicy Bypass -File "tools\package_engine.ps1"
if errorlevel 1 (
    echo Erro ao empacotar distribuicao!
    exit /b 1
)

echo 3. Compilando o Instalador Autonomo (OverflowEngine_Setup.exe)...
call "tools\build_installer.bat"
if errorlevel 1 (
    echo Erro ao compilar instalador!
    exit /b 1
)

echo ========================================================
echo  SUCESSO! Instalador pronto em dist\OverflowEngine_Setup.exe
echo ========================================================
