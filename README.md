# Overflow Engine 3D

Um motor de jogo 3D de alta performance desenvolvido do zero em **C++20** por um único desenvolvedor (*solo dev*), focado em arquitetura modular de componentes, renderização moderna de alta fidelidade e ferramentas de edição em tempo real.

---

## Visão do Projeto
- **Paradigma Amigável & Performático:** Arquitetura orientada a componentes (estilo Unity GameObjects/Components) implementada com gerenciamento de memória em C++ moderno.
- **RHI (Render Hardware Interface):** Abstração de baixo nível para permitir suporte a múltiplas APIs gráficas (iniciando em OpenGL 4.6 Core Profile com visão para Vulkan/DirectX 12).
- **Fidelidade Gráfica de Ponta:** Pipeline PBR (Physically Based Rendering - Cook-Torrance GGX), mapeamento de sombras com PCF, iluminação IBL e pós-processamento HDR.
- **Overflow Editor:** Editor em tempo real construído com Dear ImGui, viewport desacoplado em Framebuffer e painéis de inspeção e métricas.

---

## Stack Tecnológica & Dependências
- **Linguagem:** C++20
- **Compilador & Build:** MSVC / Clang / GCC via **CMake (3.22+)**
- **Janela & Contexto:** GLFW 3.4
- **Loader Gráfico:** GLAD (OpenGL 4.6 Core Profile)
- **Matemática:** GLM (OpenGL Mathematics)
- **Logs:** spdlog
- **GUI do Editor:** Dear ImGui
- **Carregamento de Imagens:** stb_image

---

## Como Compilar e Executar

### Pré-requisitos
- Visual Studio 2022 (com suporte a desenvolvimento Desktop em C++) ou Clang/GCC moderno.
- CMake 3.22 ou superior.

### Linha de comando
```bash
# 1. Gerar arquivos de projeto CMake
cmake -B build -S .

# 2. Compilar o Editor da Overflow Engine
cmake --build build --config Release

# 3. Executar
./build/bin/Release/OverflowEditor.exe
```

---

## Roadmap de Desenvolvimento
- [x] **Fase 1:** Setup do repositório, CMake, RHI inicial e janela GLFW com OpenGL 4.6 Core.
- [ ] **Fase 2:** Abstrações de GPU (`Shader`, `VertexBuffer`, `IndexBuffer`, `VertexArray`) e Câmera 3D interativa.
- [ ] **Fase 3:** Editor ImGui com viewport em Framebuffer independente.
- [ ] **Fase 4:** Importação de malhas 3D (`.obj`/`.gltf`) e sistema de texturas.
- [ ] **Fase 5:** Grafo de Cena e arquitetura de componentes (Transform, MeshRenderer, Luzes).
- [ ] **Fase 6:** Iluminação PBR (Cook-Torrance GGX), Sombras e Pós-Processamento.
