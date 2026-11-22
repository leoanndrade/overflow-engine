# -----------------------------------------------------------------------------
# Overflow Engine - Script de Empacotamento de Distribuicao Oficial
# Gera a pasta 'dist/OverflowEngine' limpa, sem expor o codigo-fonte C++ interno.
# -----------------------------------------------------------------------------

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$DistDir = Join-Path $RootDir "dist\OverflowEngine"
$BinSource = Join-Path $RootDir "build\bin\Release\OverflowEditor.exe"

Write-Host "Iniciando empacotamento da Overflow Engine..." -ForegroundColor Cyan

# 1. Cria a estrutura de pastas
if (Test-Path $DistDir) {
    Remove-Item $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path "$DistDir\bin" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\EngineAssets\textures" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\EngineAssets\models" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\include\Engine\Scene" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\include\Engine\Core" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\include\Engine\Project" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\include\Engine\Renderer" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\include\glm" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\SDK\lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\ProjectTemplate\assets\scenes" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\ProjectTemplate\assets\scripts" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\ProjectTemplate\assets\textures" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\ProjectTemplate\assets\models" -Force | Out-Null

# 2. Copia o Executavel Principal do Editor
Write-Host "Copiando binario do Editor..." -ForegroundColor Yellow
Copy-Item $BinSource "$DistDir\bin\OverflowEditor.exe" -Force

# 3. Copia Assets do Motor
Write-Host "Copiando EngineAssets..." -ForegroundColor Yellow
Copy-Item "$RootDir\assets\textures\uv_checker.bmp" "$DistDir\EngineAssets\textures\" -Force
if (Test-Path "$RootDir\assets\models\torus.obj") {
    Copy-Item "$RootDir\assets\models\torus.obj" "$DistDir\EngineAssets\models\" -Force
}

# 4. Copia os Headers do SDK (apenas os .h necessarios para o usuario compilar scripts OBehaviour)
Write-Host "Copiando SDK Headers (codigo .cpp permanece 100% protegido)..." -ForegroundColor Yellow
Copy-Item "$RootDir\src\Engine\Scene\ScriptableEntity.h" "$DistDir\SDK\include\Engine\Scene\" -Force
Copy-Item "$RootDir\src\Engine\Scene\Component.h" "$DistDir\SDK\include\Engine\Scene\" -Force
Copy-Item "$RootDir\src\Engine\Scene\Entity.h" "$DistDir\SDK\include\Engine\Scene\" -Force
Copy-Item "$RootDir\src\Engine\Scene\Scene.h" "$DistDir\SDK\include\Engine\Scene\" -Force
Copy-Item "$RootDir\src\Engine\Scene\ScriptRegistry.h" "$DistDir\SDK\include\Engine\Scene\" -Force
Copy-Item "$RootDir\src\Engine\Core\Timestep.h" "$DistDir\SDK\include\Engine\Core\" -Force
Copy-Item "$RootDir\src\Engine\Core\Input.h" "$DistDir\SDK\include\Engine\Core\" -Force
Copy-Item "$RootDir\src\Engine\Core\KeyCodes.h" "$DistDir\SDK\include\Engine\Core\" -Force
Copy-Item "$RootDir\src\Engine\Core\MouseCodes.h" "$DistDir\SDK\include\Engine\Core\" -Force
Copy-Item "$RootDir\src\Engine\Core\Log.h" "$DistDir\SDK\include\Engine\Core\" -Force
Copy-Item "$RootDir\src\Engine\Project\Project.h" "$DistDir\SDK\include\Engine\Project\" -Force
Copy-Item "$RootDir\src\Engine\Renderer\Texture.h" "$DistDir\SDK\include\Engine\Renderer\" -Force
Copy-Item "$RootDir\src\Engine\Renderer\Model.h" "$DistDir\SDK\include\Engine\Renderer\" -Force

# Copia GLM para o SDK
Copy-Item "$RootDir\vendor\glm\glm" "$DistDir\SDK\include\" -Recurse -Force

# Copia as bibliotecas compiladas (.lib) para vinculacao de scripts externos
Copy-Item "$RootDir\build\lib\Release\OverflowEngine.lib" "$DistDir\SDK\lib\" -Force

# 5. Gera o Template de Projeto Padrao
Write-Host "Criando ProjectTemplate..." -ForegroundColor Yellow
$TemplateProjectContent = @"
# Overflow Engine Project File
Name: "MyNewGame"
EngineVersion: "1.0.0"
StartScene: "assets/scenes/Main.scene"
AssetDirectory: "assets"
ScriptDirectory: "assets/scripts"
"@
Set-Content -Path "$DistDir\ProjectTemplate\MyNewGame.ovproject" -Value $TemplateProjectContent

$PlayerScriptContent = @"
#pragma once

#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Component.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/KeyCodes.h"
#include "Engine/Core/Log.h"

namespace Overflow
{
    class PlayerController : public OBehaviour
    {
    public:
        float Speed = 5.0f;

    protected:
        void OnInit() override
        {
            OVERFLOW_INFO("PlayerController inicializado no mundo!");
        }

        void OnTick(Timestep ts) override
        {
            if (!HasComponent<TransformComponent>())
            {
                return;
            }
            auto& tc = GetComponent<TransformComponent>();

            if (Input::IsKeyPressed(Key::W)) tc.Translation.z -= Speed * ts.GetSeconds();
            if (Input::IsKeyPressed(Key::S)) tc.Translation.z += Speed * ts.GetSeconds();
            if (Input::IsKeyPressed(Key::A)) tc.Translation.x -= Speed * ts.GetSeconds();
            if (Input::IsKeyPressed(Key::D)) tc.Translation.x += Speed * ts.GetSeconds();
        }
    };
}
"@
Set-Content -Path "$DistDir\ProjectTemplate\assets\scripts\PlayerController.h" -Value $PlayerScriptContent
Copy-Item "$RootDir\assets\textures\uv_checker.bmp" "$DistDir\ProjectTemplate\assets\textures\" -Force
if (Test-Path "$RootDir\assets\scenes\demo.scene") {
    Copy-Item "$RootDir\assets\scenes\demo.scene" "$DistDir\ProjectTemplate\assets\scenes\Main.scene" -Force
}

# 6. Compacta o payload para o instalador autônomo
Write-Host "Compactando pacote para o instalador..." -ForegroundColor Yellow
$PayloadZip = Join-Path $RootDir "dist\payload.zip"
$InstallerZip = Join-Path $RootDir "installer\payload.zip"
if (Test-Path $PayloadZip) { Remove-Item $PayloadZip -Force }
if (Test-Path $InstallerZip) { Remove-Item $InstallerZip -Force }

tar.exe -a -cf "$PayloadZip" -C "$DistDir" *
Copy-Item $PayloadZip $InstallerZip -Force

Write-Host "Empacotamento concluido com sucesso em '$DistDir'!" -ForegroundColor Green
$zipLength = (Get-Item -LiteralPath $PayloadZip).Length
$zipMB = [math]::Round($zipLength / 1MB, 2)
Write-Host "Arquivo de payload gerado em '$PayloadZip' ($zipMB MB)" -ForegroundColor Green
