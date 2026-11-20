# Custom 3D Game Engine (Estudo & Desenvolvimento)

Motor de jogo 3D autônomo desenvolvido do zero em **C++20**, focado em arquitetura limpa de componentes, pipeline de renderização moderna e ferramentas de edição em tempo real.

---

## 🎯 Objetivos do Projeto
- **Arquitetura Orientada a Componentes:** Estruturação de cenas intuitiva inspirada no paradigma de GameObjects/Components.
- **RHI (Render Hardware Interface):** Abstração do pipeline gráfico para permitir múltiplas APIs (OpenGL 4.5+ DSA inicialmente, com expansão para Vulkan/DirectX).
- **Pipeline de Renderização de Alta Fidelidade:** PBR (Physically Based Rendering), Sombras Dinâmicas, Framebuffer HDR, Tone Mapping e IBL.
- **Editor Integrado:** Manipulação de cenas, painel de inspeção de propriedades e métricas via Dear ImGui em um viewport desacoplado.

---

## 🛠️ Stack Tecnológica & Dependências
- **Linguagem:** C++20
- **Compilador & Build:** MSVC / Clang / GCC via **CMake (3.22+)**
- **Janela & Contexto:** GLFW 3.4
- **Loader Gráfico:** GLAD (OpenGL 4.5 Core Profile)
- **Matemática:** GLM (OpenGL Mathematics)
- **Logs:** spdlog
- **GUI do Editor:** Dear ImGui
- **Carregamento de Imagens:** stb_image

---

## 🚀 Como Compilar e Executar

### Pré-requisitos
- Visual Studio 2022 (com suporte a desenvolvimento Desktop em C++) ou Clang/GCC moderno.
- CMake 3.22 ou superior.

### Linha de comando
```bash
# Gerar arquivos de projeto CMake
cmake -B build -S .

# Compilar em modo Release
cmake --build build --config Release

# Executar a Engine / Editor
./build/bin/Release/CustomEditor.exe
```

---

## 🗺️ Roadmap de Desenvolvimento
- [x] **Fase 1:** Setup do repositório, CMake e criação da janela inicial com contexto OpenGL.
- [ ] **Fase 2:** Abstrações de GPU (`Shader`, `VertexBuffer`, `IndexBuffer`, `VertexArray`) e Câmera 3D interativa.
- [ ] **Fase 3:** Editor ImGui com viewport em Framebuffer independente.
- [ ] **Fase 4:** Importação de malhas 3D (`.obj`/`.gltf`) e sistema de texturas.
- [ ] **Fase 5:** Grafo de Cena e arquitetura de componentes (Transform, MeshRenderer, Luzes).
- [ ] **Fase 6:** Iluminação PBR (Cook-Torrance GGX), Sombras e Pós-Processamento.
