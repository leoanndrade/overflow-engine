#include "EditorLayer.h"
#include <glad/gl.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Input.h>
#include <Engine/Core/KeyCodes.h>
#include <Engine/Core/MouseCodes.h>
#include <Engine/Core/Log.h>
#include <Engine/Renderer/RenderCommand.h>
#include <Engine/Audio/AudioEngine.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scene/ScriptManager.h>
#include <Engine/Project/Project.h>
#include "EditorPreferences.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

EditorLayer::EditorLayer()
    : Layer("EditorLayer"),
      m_Camera(45.0f, 1.778f, 0.1f, 1000.0f) {
}

void EditorLayer::OnAttach() {
    OVERFLOW_INFO("Iniciando Overflow Engine Editor...");
    Overflow::AudioEngine::Init();
    Overflow::ScriptManager::Init();
    Overflow::EditorPreferences::Init();

    std::filesystem::path projectToOpen;
    auto& args = Overflow::Application::Get().GetCommandLineArgs();
    if (args.Count > 1) {
        std::filesystem::path passedPath = args[1];
        if (std::filesystem::exists(passedPath) && passedPath.extension() == ".ovproject") {
            projectToOpen = passedPath;
        }
    }

    if (projectToOpen.empty()) {
        std::filesystem::path defaultProject = "OverflowEngine.ovproject";
        if (std::filesystem::exists(defaultProject)) {
            projectToOpen = defaultProject;
        }
    }

    if (!projectToOpen.empty()) {
        OpenProject(projectToOpen.string());
    } else {
        auto project = Overflow::Project::NewProject(".", "OverflowEngine");
        if (project) {
            m_ContentBrowserPanel.SetBaseDirectory(project->GetAssetDirectory());
        }
    }

    m_ContentBrowserPanel.SetOpenSceneCallback([this](const std::string& path) {
        OpenScene(path);
    });

    m_ContentBrowserPanel.SetInstantiateArchetypeCallback([this](const std::string& path) {
        auto entity = Overflow::SceneSerializer::DeserializeEntity(path, m_ActiveScene.get());
        if (entity) {
            m_SceneHierarchyPanel.SetSelectedEntity(entity);
            ShowToast("Archetype instanciado no mundo: " + std::filesystem::path(path).filename().string(), ImVec4(0.35f, 0.90f, 0.50f, 1.0f));
        }
    });

    m_Camera.SetPosition({ 0.0f, 3.5f, 7.5f });
    m_Camera.SetRotation(-22.0f, -90.0f);

    // 1. Framebuffer Multi-Target (MRT): Anexo 0 (RGBA8), Anexo 1 (R32I EntityID), Depth (24_8)
    Overflow::FramebufferSpecification fbSpec;
    fbSpec.Attachments = {
        Overflow::FramebufferTextureFormat::RGBA8,
        Overflow::FramebufferTextureFormat::RED_INTEGER,
        Overflow::FramebufferTextureFormat::Depth
    };
    fbSpec.Width = 1280;
    fbSpec.Height = 720;
    m_Framebuffer = Overflow::Framebuffer::Create(fbSpec);

    // Framebuffer dedicado para a Janela Game (Game View)
    Overflow::FramebufferSpecification gameFbSpec;
    gameFbSpec.Attachments = {
        Overflow::FramebufferTextureFormat::RGBA8,
        Overflow::FramebufferTextureFormat::Depth
    };
    gameFbSpec.Width = 1280;
    gameFbSpec.Height = 720;
    m_GameFramebuffer = Overflow::Framebuffer::Create(gameFbSpec);

    // 2. Compilação do Shader Básico
    std::string vertexSrc = R"(
        #version 460 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec2 a_TexCoord;
        layout(location = 3) in vec4 a_Color;

        uniform mat4 u_ViewProjection;
        uniform mat4 u_Transform;

        out vec3 v_Normal;
        out vec2 v_TexCoord;
        out vec4 v_Color;
        out vec3 v_FragPos;

        void main() {
            vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
            v_FragPos = worldPos.xyz;
            v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
            v_TexCoord = a_TexCoord;
            v_Color = a_Color;
            gl_Position = u_ViewProjection * worldPos;
        }
    )";

    std::string fragmentSrc = R"(
        #version 460 core

        in vec3 v_Normal;
        in vec2 v_TexCoord;
        in vec4 v_Color;
        in vec3 v_FragPos;

        uniform sampler2D u_Texture;
        uniform float u_Tiling;
        uniform vec3 u_LightDirection;
        uniform float u_AmbientIntensity;
        uniform vec4 u_BaseColor;

        layout(location = 0) out vec4 o_Color;
        layout(location = 1) out int o_EntityID;

        uniform int u_EntityID;

        void main() {
            vec3 lightDir = normalize(u_LightDirection);
            vec3 normal = normalize(v_Normal);

            float diff = max(dot(normal, lightDir), 0.0);

            vec4 texColor = texture(u_Texture, v_TexCoord * u_Tiling);
            vec3 base = texColor.rgb * v_Color.rgb * u_BaseColor.rgb;
            vec3 ambient = u_AmbientIntensity * base;
            vec3 diffuse = diff * base;

            vec3 finalColor = ambient + diffuse;
            o_Color = vec4(finalColor, texColor.a * v_Color.a * u_BaseColor.a);
            o_EntityID = u_EntityID;
        }
    )";

    m_Shader = Overflow::Shader::Create("SceneEntityShader", vertexSrc, fragmentSrc);

    // 3. Inicialização do Grafo de Cena com Pipeline PBR, Sombras e Grade
    m_ActiveScene = std::make_shared<Overflow::Scene>();
    m_SceneHierarchyPanel.SetContext(m_ActiveScene);

    CreateDefaultScene();
}

void EditorLayer::CreateDefaultScene() {
    m_ActiveScene->Clear();

    // 1. Sol / Luz Direcional com Sombras Ativas
    auto sun = m_ActiveScene->CreateDirectionalLight("Sun (Directional Light)", { 0.6f, 1.0f, 0.4f });
    auto& sunLight = sun.GetComponent<Overflow::LightComponent>();
    sunLight.Intensity = 2.5f;
    sunLight.AmbientIntensity = 0.15f;
    sunLight.CastShadows = true;
    sunLight.Bias = 0.003f;

    // 2. Chão com Textura Quadriculada e Difusão PBR + Física Estática
    auto ground = m_ActiveScene->CreatePlane("Ground Plane", { 0.0f, -0.5f, 0.0f });
    auto& groundMesh = ground.GetComponent<Overflow::MeshComponent>();
    groundMesh.TextureAsset = Overflow::Texture2D::Create("assets/textures/uv_checker.bmp");
    groundMesh.Tiling = 8.0f;
    groundMesh.Color = { 0.85f, 0.85f, 0.90f, 1.0f };
    groundMesh.Roughness = 0.85f;
    groundMesh.Metallic = 0.0f;
    auto& groundRB = ground.AddComponent<Overflow::RigidbodyComponent>();
    groundRB.Type = Overflow::BodyType::Static;
    auto& groundCol = ground.AddComponent<Overflow::ColliderComponent>(Overflow::ColliderType::Box);
    groundCol.Size = { 12.0f, 0.2f, 12.0f };

    // 3. Cubo Principal (Dielétrico Vermelho) + Física Dinâmica
    auto cube = m_ActiveScene->CreateCube("Red Cube", { -2.5f, 3.0f, 0.0f });
    auto& cubeMesh = cube.GetComponent<Overflow::MeshComponent>();
    cubeMesh.Color = { 0.95f, 0.25f, 0.20f, 1.0f };
    cubeMesh.Roughness = 0.35f;
    cubeMesh.Metallic = 0.05f;
    auto& cubeRB = cube.AddComponent<Overflow::RigidbodyComponent>();
    cubeRB.Mass = 1.5f;
    auto& cubeCol = cube.AddComponent<Overflow::ColliderComponent>(Overflow::ColliderType::Box);
    cubeCol.Bounciness = 0.4f;

    // 4. Esfera Central Cromada/Metálica (Reflexos Cook-Torrance GGX) + Física Dinâmica
    auto sphere = m_ActiveScene->CreateSphere("Metallic Sphere", { 0.0f, 4.5f, 0.0f });
    auto& sphereMesh = sphere.GetComponent<Overflow::MeshComponent>();
    sphereMesh.TextureAsset = Overflow::Texture2D::Create("assets/textures/uv_checker.bmp");
    sphereMesh.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    sphereMesh.Roughness = 0.12f;
    sphereMesh.Metallic = 0.95f;
    auto& sphereRB = sphere.AddComponent<Overflow::RigidbodyComponent>();
    sphereRB.Mass = 2.0f;
    auto& sphereCol = sphere.AddComponent<Overflow::ColliderComponent>(Overflow::ColliderType::Sphere);
    sphereCol.Radius = 0.5f;
    sphereCol.Bounciness = 0.75f; // Quique elástico acentuado!

    // 5. Câmera Principal da Cena para a Janela Game (Game View)
    auto mainCam = m_ActiveScene->CreateEntity("Main Camera");
    mainCam.GetComponent<Overflow::TransformComponent>().Translation = { 0.0f, 3.5f, 9.0f };
    mainCam.GetComponent<Overflow::TransformComponent>().Rotation = { -15.0f, 0.0f, 0.0f };
    auto& camComp = mainCam.AddComponent<Overflow::CameraComponent>();
    camComp.Primary = true;
    camComp.Camera.SetPosition({ 0.0f, 3.5f, 9.0f });
    camComp.Camera.SetRotation(-15.0f, -90.0f);

    // 5. Torus 3D (.obj Metálico Ciano)
    auto torus = m_ActiveScene->CreateEntity("Torus 3D (.obj)");
    torus.GetComponent<Overflow::TransformComponent>().Translation = { 2.5f, 0.5f, 0.0f };
    auto& torusMesh = torus.AddComponent<Overflow::MeshComponent>(Overflow::MeshGeometryType::Torus);
    torusMesh.ModelAsset = m_ActiveScene->GetSharedTorus();
    torusMesh.Color = { 0.20f, 0.80f, 0.95f, 1.0f };
    torusMesh.Roughness = 0.25f;
    torusMesh.Metallic = 0.90f;

    // 6. Satélite Filho da Esfera (Grafo de Cena)
    auto satellite = m_ActiveScene->CreateCube("Satellite Child", { 0.0f, 1.5f, 0.0f });
    satellite.GetComponent<Overflow::TransformComponent>().Scale = { 0.35f, 0.35f, 0.35f };
    satellite.GetComponent<Overflow::TransformComponent>().ParentID = sphere.GetID();
    auto& satMesh = satellite.GetComponent<Overflow::MeshComponent>();
    satMesh.Color = { 1.0f, 0.85f, 0.20f, 1.0f };
    satMesh.Roughness = 0.30f;
    satMesh.Metallic = 0.50f;

    // 7. Point Light Decorativa (Luz Dourada)
    auto pointLight = m_ActiveScene->CreateEntity("Point Light (Warm)");
    pointLight.GetComponent<Overflow::TransformComponent>().Translation = { -2.0f, 2.5f, 1.5f };
    auto& plComp = pointLight.AddComponent<Overflow::LightComponent>();
    plComp.Type = Overflow::LightType::Point;
    plComp.Color = { 1.0f, 0.70f, 0.35f };
    plComp.Intensity = 4.0f;
    plComp.Range = 15.0f;

    // 8. Efeito de Particulas (Chafariz de Faiscas Douradas)
    auto particles = m_ActiveScene->CreateEntity("Spark Particle Fountain");
    particles.GetComponent<Overflow::TransformComponent>().Translation = { 2.5f, 0.0f, 0.0f };
    auto& ps = particles.AddComponent<Overflow::ParticleSystemComponent>();
    ps.StartColor = { 1.0f, 0.75f, 0.2f, 1.0f };
    ps.EndColor = { 1.0f, 0.15f, 0.05f, 0.0f };
    ps.BaseVelocity = { 0.0f, 2.8f, 0.0f };
    ps.Gravity = { 0.0f, -4.0f, 0.0f };
    ps.EmissionRate = 35.0f;
    ps.Lifetime = 1.2f;
    ps.StartSize = 0.15f;
    ps.EndSize = 0.02f;

    m_SceneHierarchyPanel.SetSelectedEntity(sphere);
}

void EditorLayer::OnDetach() {
    Overflow::AudioEngine::Shutdown();
    Overflow::ScriptManager::Shutdown();
    OVERFLOW_INFO("Overflow Editor Layer finalizado.");
}

void EditorLayer::OnUpdate(Overflow::Timestep ts) {
    m_LastFrameTimeMs = ts.GetMilliseconds();
    m_FPS = (ts.GetSeconds() > 0.0f) ? (1.0f / ts.GetSeconds()) : 0.0f;

    m_FPSHistory[m_FPSHistoryIndex] = m_FPS;
    m_FPSHistoryIndex = (m_FPSHistoryIndex + 1) % 60;

    // Recarregar Scripts (Hot-Reload F5)
    if (Overflow::Input::IsKeyPressed(Overflow::Key::F5)) {
        Overflow::ScriptManager::ReloadScriptsDLL();
        ShowToast("Scripts C++ recarregados (Live Coding / Hot-Reload)!", ImVec4(0.35f, 0.90f, 0.50f, 1.0f));
    }

    // Atualiza parâmetros de pós-processamento e grade da cena ativa
    if (m_ActiveScene) {
        m_ActiveScene->SetExposure(m_Exposure);
        m_ActiveScene->SetShowGrid(m_ShowGrid);
        m_ActiveScene->SetSelectedEntity(m_SceneHierarchyPanel.GetSelectedEntity() ? m_SceneHierarchyPanel.GetSelectedEntity().GetID() : 0);
    }

    // Atualização dos temporizadores das notificações toast
    for (auto it = m_Toasts.begin(); it != m_Toasts.end();) {
        it->RemainingSeconds -= ts.GetSeconds();
        if (it->RemainingSeconds <= 0.0f) {
            it = m_Toasts.erase(it);
        } else {
            ++it;
        }
    }

    // Atalhos Globais: Ctrl+N, Ctrl+S, Ctrl+O, Ctrl+D, Ctrl+Z (Undo), Ctrl+Y (Redo)
    bool ctrl = Overflow::Input::IsKeyPressed(Overflow::Key::LeftControl) || Overflow::Input::IsKeyPressed(Overflow::Key::RightControl);
    bool shift = Overflow::Input::IsKeyPressed(Overflow::Key::LeftShift) || Overflow::Input::IsKeyPressed(Overflow::Key::RightShift);
    if (ctrl) {
        if (Overflow::Input::IsKeyPressed(Overflow::Key::N)) NewScene();
        if (Overflow::Input::IsKeyPressed(Overflow::Key::S)) SaveScene();
        if (Overflow::Input::IsKeyPressed(Overflow::Key::O)) OpenScene();
        if (Overflow::Input::IsKeyPressed(Overflow::Key::D)) DuplicateSelectedEntity();

        static bool zDown = false;
        if (Overflow::Input::IsKeyPressed(Overflow::Key::Z)) {
            if (!zDown) {
                if (shift) Redo();
                else Undo();
            }
            zDown = true;
        } else {
            zDown = false;
        }

        static bool yDown = false;
        if (Overflow::Input::IsKeyPressed(Overflow::Key::Y)) {
            if (!yDown) Redo();
            yDown = true;
        } else {
            yDown = false;
        }
    }

    if (m_ViewportFocused || m_ViewportHovered || m_SceneHierarchyPanel.IsHierarchyFocused()) {
        if (Overflow::Input::IsKeyPressed(Overflow::Key::Delete)) {
            DeleteSelectedEntity();
        }
        if (Overflow::Input::IsKeyPressed(Overflow::Key::F)) {
            FocusSelectedEntity();
        }
    }

    if (m_SceneHierarchyPanel.CheckEntityDoubleClicked()) {
        FocusSelectedEntity();
    }

    // 1. Redimensionamento do Framebuffer quando a aba do Viewport muda de tamanho
    const auto& spec = m_Framebuffer->GetSpecification();
    if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
        (spec.Width != static_cast<uint32_t>(m_ViewportSize.x) || spec.Height != static_cast<uint32_t>(m_ViewportSize.y))) {
        m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
        m_Camera.SetViewportSize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
        m_ActiveScene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
    }

    if (m_GameFramebuffer) {
        const auto& gameSpec = m_GameFramebuffer->GetSpecification();
        if (m_GameViewportSize.x > 0.0f && m_GameViewportSize.y > 0.0f &&
            (gameSpec.Width != static_cast<uint32_t>(m_GameViewportSize.x) || gameSpec.Height != static_cast<uint32_t>(m_GameViewportSize.y))) {
            m_GameFramebuffer->Resize(static_cast<uint32_t>(m_GameViewportSize.x), static_cast<uint32_t>(m_GameViewportSize.y));
        }
    }

    // 2. Navegação Completa da Câmera estilo Unity & Unreal (Flycam com Cursor Oculto e Ilimitado, Orbit, Pan, Zoom)
    bool isRightMouseDown = Overflow::Input::IsMouseButtonPressed(Overflow::Mouse::ButtonRight);
    bool isLeftMouseDown = Overflow::Input::IsMouseButtonPressed(Overflow::Mouse::ButtonLeft);
    bool isMiddleMouseDown = Overflow::Input::IsMouseButtonPressed(Overflow::Mouse::ButtonMiddle);
    bool isAltDown = Overflow::Input::IsKeyPressed(Overflow::Key::LeftAlt) || Overflow::Input::IsKeyPressed(Overflow::Key::RightAlt);

    glm::vec2 currentMousePos = Overflow::Input::GetMousePosition();
    glm::vec2 mouseDelta = currentMousePos - m_LastViewportMousePos;
    m_LastViewportMousePos = currentMousePos;

    // Inicia Flycam: esconde o mouse e permite rotação contínua de 360 graus sem bater na borda da tela
    if (isRightMouseDown && m_ViewportHovered && !m_IsCameraFlying) {
        m_IsCameraFlying = true;
        m_Camera.ResetMouseState();
        Overflow::Input::SetCursorMode(true);
    }
    if (!isRightMouseDown && m_IsCameraFlying) {
        m_IsCameraFlying = false;
        m_Camera.ResetMouseState();
        Overflow::Input::SetCursorMode(false);
    }

    // Inicia Orbit se Alt + Botão Esquerdo
    if (isAltDown && isLeftMouseDown && m_ViewportHovered && !ImGuizmo::IsUsing()) {
        m_IsCameraOrbiting = true;
    }
    if (!isLeftMouseDown || !isAltDown) {
        m_IsCameraOrbiting = false;
    }

    // Inicia Pan se Botão do Meio
    if ((isMiddleMouseDown || (isAltDown && isMiddleMouseDown)) && m_ViewportHovered) {
        m_IsCameraPanning = true;
    }
    if (!isMiddleMouseDown) {
        m_IsCameraPanning = false;
    }

    // Executa os controles da Câmera
    if (m_IsCameraFlying) {
        m_Camera.OnUpdate(ts);
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_Camera.SetMoveSpeed(std::clamp(m_Camera.GetMoveSpeed() + wheel * 1.5f, 0.5f, 100.0f));
        }
    } else if (m_IsCameraOrbiting) {
        m_Camera.Orbit(mouseDelta);
    } else if (m_IsCameraPanning) {
        m_Camera.Pan(mouseDelta);
    } else if (m_ViewportHovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_Camera.Zoom(wheel);
        }
    }

    // Atalhos de Gizmo estilo Unity/Unreal: Q (Select), W (Translate), E (Rotate), R (Scale), Space (Local/World)
    if ((m_ViewportFocused || m_ViewportHovered) && !isRightMouseDown && !isAltDown && !ctrl) {
        if (Overflow::Input::IsKeyPressed(Overflow::Key::Q)) m_GizmoType = -1;
        if (Overflow::Input::IsKeyPressed(Overflow::Key::W)) m_GizmoType = 0; // Translate
        if (Overflow::Input::IsKeyPressed(Overflow::Key::E)) m_GizmoType = 1; // Rotate
        if (Overflow::Input::IsKeyPressed(Overflow::Key::R)) m_GizmoType = 2; // Scale
        if (Overflow::Input::IsKeyPressed(Overflow::Key::Space)) {
            static bool spacePressedLastFrame = false;
            if (!spacePressedLastFrame) {
                m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
            }
            spacePressedLastFrame = true;
        } else {
            static bool spacePressedLastFrame = false;
            spacePressedLastFrame = false;
        }
    }

    // 3. Atualização lógica da Cena (Modo Editor vs Modo Play com Física 3D)
    switch (m_SceneState) {
        case SceneState::Edit:
            m_ActiveScene->OnUpdate(ts);
            break;
        case SceneState::Play:
            m_ActiveScene->OnUpdateRuntime(ts);
            break;
        case SceneState::Pause:
            break;
    }

    // 4. Renderização da Viewport 3D do Editor
    if (m_WireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    m_ActiveScene->OnRenderEditor(m_Camera, m_Framebuffer);

    if (m_WireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // 5. Renderização da Janela Game a partir da Câmera Principal da Cena
    if (m_ShowGameView && m_GameFramebuffer) {
        if (m_ActiveScene->HasActiveCamera()) {
            auto camEntity = m_ActiveScene->GetPrimaryCameraEntity();
            if (camEntity && camEntity.HasComponent<Overflow::CameraComponent>()) {
                auto& cc = camEntity.GetComponent<Overflow::CameraComponent>();
                const auto& tc = camEntity.GetComponent<Overflow::TransformComponent>();
                cc.Camera.SetPosition(tc.Translation);
                cc.Camera.SetRotation(tc.Rotation.x, cc.Camera.GetYaw());
                cc.Camera.SetViewportSize(static_cast<uint32_t>(m_GameViewportSize.x), static_cast<uint32_t>(m_GameViewportSize.y));
                m_ActiveScene->OnRenderEditor(cc.Camera, m_GameFramebuffer);
            }
        } else {
            m_GameFramebuffer->Bind();
            Overflow::RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.09f, 1.0f });
            Overflow::RenderCommand::Clear();
            m_GameFramebuffer->Unbind();
        }
    }
}

void EditorLayer::OnImGuiRender() {
    RenderDockSpace();
    RenderToolbar();

    if (m_ShowViewport)
        RenderViewportPanel();

    if (m_ShowGameView)
        RenderGamePanel();

    if (m_ShowHierarchy || m_ShowInspector)
        m_SceneHierarchyPanel.OnImGuiRender();

    if (m_ShowContentBrowser)
        m_ContentBrowserPanel.OnImGuiRender(&m_ShowContentBrowser);

    if (m_ShowConsole)
        m_ConsolePanel.OnImGuiRender(&m_ShowConsole);

    if (m_ShowMetrics)
        RenderMetricsPanel();

    if (m_ShowAboutModal)
        RenderAboutModal();

    if (m_ShowPreferencesModal)
        RenderPreferencesModal();

    RenderNewProjectModal();

    if (m_ShowCreateScriptModal)
        RenderCreateScriptModal();

    RenderStatusBar();
    RenderToastNotifications();
}

void EditorLayer::RenderDockSpace() {
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    float toolbarHeight = 36.0f;
    float statusBarHeight = 26.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen) {
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight - statusBarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    bool dockspaceOpen = true;
    ImGui::Begin("OverflowDockSpaceHost", &dockspaceOpen, window_flags);

    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("OverflowDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        static bool s_FirstInit = true;
        if (s_FirstInit || m_ResetLayout) {
            s_FirstInit = false;
            m_ResetLayout = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight - statusBarHeight));

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("World Outliner", dock_id_left);
            ImGui::DockBuilderDockWindow("Entity Details", dock_id_right);
            ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Performance & Metrics", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Scene Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Live Simulation", dock_main_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    RenderMenuBar();

    ImGui::End();
}

void EditorLayer::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...")) {
                m_ShowNewProjectModal = true;
            }
            if (ImGui::MenuItem("Open Project...")) {
                OpenProject();
            }
            if (ImGui::MenuItem("Save Project", nullptr, false, Overflow::Project::GetActive() != nullptr)) {
                SaveProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                NewScene();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                OpenScene();
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                SaveScene();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Standalone Game...")) {
                SaveScene("assets/scenes/standalone_build.scene");
                ShowToast("Exportacao Standalone concluida em 'assets/scenes/standalone_build.scene'!", ImVec4(0.3f, 0.9f, 0.45f, 1.0f), 5.0f);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Scene")) {
                m_ActiveScene->Clear();
                m_SceneHierarchyPanel.SetSelectedEntity({});
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                Overflow::Application::Get().Close();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, !m_UndoStack.empty())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !m_RedoStack.empty())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences & External Tools...")) {
                m_ShowPreferencesModal = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets")) {
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("C++ Script...")) {
                    m_ShowCreateScriptModal = true;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open Project in JetBrains Rider")) {
                Overflow::ScriptManager::OpenInRider();
                ShowToast("Iniciando JetBrains Rider...", ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
            }
            if (ImGui::MenuItem("Configure Preferred IDE...")) {
                m_ShowPreferencesModal = true;
            }
            if (ImGui::MenuItem("Reload Scripts (Live Coding)", "F5")) {
                Overflow::ScriptManager::ReloadScriptsDLL();
                ShowToast("Scripts C++ recarregados com sucesso!", ImVec4(0.35f, 0.90f, 0.50f, 1.0f));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Viewport", nullptr, &m_ShowViewport);
            ImGui::MenuItem("Live Simulation", nullptr, &m_ShowGameView);
            ImGui::MenuItem("World Outliner", nullptr, &m_ShowHierarchy);
            ImGui::MenuItem("Entity Details", nullptr, &m_ShowInspector);
            ImGui::MenuItem("Content Browser", nullptr, &m_ShowContentBrowser);
            ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
            ImGui::MenuItem("Performance & Metrics", nullptr, &m_ShowMetrics);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Reset to Overflow Studio Layout")) {
                m_ResetLayout = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Overflow Engine")) {
                m_ShowAboutModal = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EditorLayer::RenderToolbar() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float toolbarHeight = 36.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration 
                                  | ImGuiWindowFlags_NoDocking 
                                  | ImGuiWindowFlags_NoMove 
                                  | ImGuiWindowFlags_NoResize 
                                  | ImGuiWindowFlags_NoScrollbar 
                                  | ImGuiWindowFlags_NoSavedSettings
                                  | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.125f, 0.14f, 1.0f));

    if (ImGui::Begin("##MainToolbar", nullptr, window_flags)) {
        float buttonHeight = toolbarHeight - 8.0f;

        // Lado Esquerdo: Identificação do Motor e Cena Ativa (estilo Unreal 5)
        ImGui::SetCursorPosY(8.0f);
        ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "OVERFLOW 3D");
        ImGui::SameLine();
        std::string sceneName = m_CurrentScenePath.empty() ? "Untitled Scene" : std::filesystem::path(m_CurrentScenePath).filename().string();
        ImGui::TextDisabled("|  %s", sceneName.c_str());

        // Centro: Botões de Simulação Play / Pause / Stop
        float centerPos = (viewport->WorkSize.x * 0.5f) - 80.0f;
        ImGui::SetCursorPosX(centerPos);
        ImGui::SetCursorPosY(4.0f);

        bool isPlay = (m_SceneState == SceneState::Play);
        bool isPause = (m_SceneState == SceneState::Pause);

        if (isPlay) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.20f, 0.9f));
            if (ImGui::Button("Stop [X]", ImVec2(75.0f, buttonHeight))) {
                OnSceneStop();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.35f, 0.9f));
            if (ImGui::Button("Play [>]", ImVec2(75.0f, buttonHeight))) {
                OnScenePlay();
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        if (isPause) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.70f, 0.20f, 0.9f));
            if (ImGui::Button("Resume [||]", ImVec2(75.0f, buttonHeight))) {
                m_SceneState = SceneState::Play;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.22f, 0.8f));
            if (ImGui::Button("Pause [||]", ImVec2(75.0f, buttonHeight))) {
                if (isPlay) OnScenePause();
            }
            ImGui::PopStyleColor();
        }

        // Lado Direito: Layout Reset e Badge de Renderizador
        ImGui::SameLine(viewport->WorkSize.x - 240.0f);
        ImGui::SetCursorPosY(6.0f);
        if (ImGui::Button("Reset Layout", ImVec2(90.0f, buttonHeight))) {
            m_ResetLayout = true;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.40f, 0.80f, 0.50f, 1.0f), "GL 4.6 Core");
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void EditorLayer::OnScenePlay() {
    m_SceneState = SceneState::Play;
    m_EditorScene = m_ActiveScene;
    m_ActiveScene = Overflow::Scene::Copy(m_EditorScene);
    m_SceneHierarchyPanel.SetContext(m_ActiveScene);
    OVERFLOW_INFO("Modo Play iniciado - Simulação de cena e física 3D em tempo real!");
    ShowToast("Simulacao iniciada (Play & Fisica)", ImVec4(0.3f, 0.85f, 0.4f, 1.0f));

    if (m_MaximizeOnPlay) {
        m_ShowGameView = true;
        ImGui::SetWindowFocus("Live Simulation");
    }
}

void EditorLayer::OnScenePause() {
    m_SceneState = SceneState::Pause;
    OVERFLOW_INFO("Modo Pause ativado.");
    ShowToast("Simulacao pausada", ImVec4(0.95f, 0.75f, 0.2f, 1.0f));
}

void EditorLayer::OnSceneStop() {
    m_SceneState = SceneState::Edit;
    if (m_ActiveScene) {
        m_ActiveScene->StopRuntimeScripts();
    }
    m_ActiveScene = m_EditorScene;
    m_EditorScene = nullptr;
    m_SceneHierarchyPanel.SetContext(m_ActiveScene);
    OVERFLOW_INFO("Modo Edit restaurado.");
    ShowToast("Modo Edit restaurado", ImVec4(0.85f, 0.4f, 0.4f, 1.0f));
}

void EditorLayer::NewScene() {
    m_ActiveScene = std::make_shared<Overflow::Scene>();
    m_SceneHierarchyPanel.SetContext(m_ActiveScene);
    m_CurrentScenePath = "";
    CreateDefaultScene();
    OVERFLOW_INFO("Nova cena criada com sucesso.");
    ShowToast("Nova cena criada com sucesso", ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
}

void EditorLayer::OpenScene(const std::string& filepath) {
    std::string path = filepath.empty() ? "assets/scenes/demo.scene" : filepath;
    if (!std::filesystem::exists(path)) {
        OVERFLOW_WARN("Arquivo de cena nao existe: '{0}'. Criando cena padrao...", path);
        SaveScene(path);
        return;
    }

    auto newScene = std::make_shared<Overflow::Scene>();
    Overflow::SceneSerializer serializer(newScene);
    if (serializer.Deserialize(path)) {
        m_ActiveScene = newScene;
        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_CurrentScenePath = path;
        ShowToast("Cena carregada: " + std::filesystem::path(path).filename().string(), ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
    }
}

void EditorLayer::SaveScene(const std::string& filepath) {
    std::string path = filepath.empty() ? (m_CurrentScenePath.empty() ? "assets/scenes/demo.scene" : m_CurrentScenePath) : filepath;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    Overflow::SceneSerializer serializer(m_ActiveScene);
    if (serializer.Serialize(path)) {
        m_CurrentScenePath = path;
        ShowToast("Cena salva com sucesso: " + std::filesystem::path(path).filename().string(), ImVec4(0.35f, 0.90f, 0.5f, 1.0f));
    }
}

void EditorLayer::NewProject() {
    m_ShowNewProjectModal = true;
}

void EditorLayer::OpenProject(const std::string& filepath) {
    std::string path = filepath;
    if (path.empty()) {
        path = "OverflowEngine.ovproject";
    }

    auto project = Overflow::Project::LoadProject(path);
    if (project) {
        m_ContentBrowserPanel.SetBaseDirectory(project->GetAssetDirectory());
        std::string startScene = project->GetConfig().StartScene;
        std::filesystem::path fullScenePath = project->GetProjectDirectory() / startScene;
        if (std::filesystem::exists(fullScenePath)) {
            OpenScene(fullScenePath.string());
        } else if (std::filesystem::exists(startScene)) {
            OpenScene(startScene);
        }
        ShowToast("Projeto aberto: " + project->GetConfig().Name, ImVec4(0.35f, 0.85f, 0.95f, 1.0f));
    }
}

void EditorLayer::SaveProject() {
    if (Overflow::Project::GetActive()) {
        Overflow::Project::SaveActive();
        ShowToast("Projeto salvo: " + Overflow::Project::GetActive()->GetConfig().Name, ImVec4(0.35f, 0.90f, 0.5f, 1.0f));
    }
}

void EditorLayer::RenderNewProjectModal() {
    if (m_ShowNewProjectModal) {
        ImGui::OpenPopup("New Project");
        m_ShowNewProjectModal = false;
    }

    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Nome do Projeto:");
        ImGui::InputText("##ProjectName", m_NewProjectName, sizeof(m_NewProjectName));

        ImGui::Spacing();
        ImGui::Text("Diretorio de Destino:");
        ImGui::InputText("##ProjectPath", m_NewProjectPath, sizeof(m_NewProjectPath));

        ImGui::Separator();
        if (ImGui::Button("Create Project", ImVec2(130.0f, 28.0f))) {
            std::filesystem::path dir = std::filesystem::path(m_NewProjectPath) / m_NewProjectName;
            auto project = Overflow::Project::NewProject(dir, m_NewProjectName);
            if (project) {
                m_ContentBrowserPanel.SetBaseDirectory(project->GetAssetDirectory());
                NewScene();
                std::filesystem::path defaultScene = project->GetAssetDirectory() / "scenes" / "Main.scene";
                SaveScene(defaultScene.string());
                ShowToast("Projeto criado com sucesso: " + project->GetConfig().Name, ImVec4(0.35f, 0.90f, 0.5f, 1.0f));
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 28.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::RenderViewportPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Scene Viewport", &m_ShowViewport, viewportFlags);

    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    // Zoom suave com o scroll do mouse diretamente na viewport
    if (m_ViewportHovered && !m_IsCameraFlying) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_Camera.Zoom(wheel);
        }
    }

    // Bounds da Viewport para cálculo de Mouse Picking
    ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
    ImVec2 viewportMaxRegion = ImGui::GetWindowContentRegionMax();
    ImVec2 viewportOffset = ImGui::GetWindowPos();
    m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
    m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

    // -------------------------------------------------------------------------
    // Toolbar Flutuante de Gizmos no Topo do Viewport (estilo Unity 6 / Unreal Engine 5)
    // -------------------------------------------------------------------------
    ImGui::SetCursorPos(ImVec2(12.0f, 32.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.105f, 0.12f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.26f, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));

    if (ImGui::BeginChild("##ViewportOverlayGizmos", ImVec2(510.0f, 34.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        auto renderGizmoBtn = [this](const char* label, int type) {
            bool active = (m_GizmoType == type);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.78f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.15f, 0.17f, 0.7f));
            }
            if (ImGui::Button(label)) m_GizmoType = type;
            ImGui::PopStyleColor();
            ImGui::SameLine();
        };

        renderGizmoBtn("Select (Q)", -1);
        renderGizmoBtn("Move (W)", 0);
        renderGizmoBtn("Rotate (E)", 1);
        renderGizmoBtn("Scale (R)", 2);

        // Botão de Foco na Câmera (F) estilo Unity / Unreal
        if (ImGui::Button("Focus (F)")) {
            FocusSelectedEntity();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Focar a câmera no objeto selecionado (F)");
        ImGui::SameLine();

        // Alternância Local / World Space
        if (ImGui::Button(m_GizmoMode == 0 ? "Local" : "World")) {
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alternar espaço de coordenadas (Local / World)");
        ImGui::SameLine();

        ImGui::Checkbox("Snap", &m_GizmoSnap);

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    // -------------------------------------------------------------------------
    // Overlay de Informações, Shading e Pós-Processamento no Canto Superior Direito da Viewport
    // -------------------------------------------------------------------------
    float rightOverlayWidth = 720.0f;
    if (m_ViewportSize.x > rightOverlayWidth + 300.0f) {
        ImGui::SetCursorPos(ImVec2(m_ViewportSize.x - rightOverlayWidth - 12.0f, 32.0f));
        if (ImGui::BeginChild("##ViewportOverlayInfo", ImVec2(rightOverlayWidth, 34.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

            // Shading Mode: Lit / Wireframe
            if (ImGui::Button(m_WireframeMode ? "Wireframe" : "Lit")) {
                m_WireframeMode = !m_WireframeMode;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alternar modo Lit / Wireframe");
            ImGui::SameLine();

            // Toggle Grade Infinita
            ImGui::Checkbox("Grid", &m_ShowGrid);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exibir grade procedural no chão");
            ImGui::SameLine();

            // Toggle Ícones 3D (Billboard Badges)
            bool showIcons = m_ActiveScene ? m_ActiveScene->GetShowGizmoIcons() : true;
            if (ImGui::Checkbox("Icons", &showIcons)) {
                if (m_ActiveScene) m_ActiveScene->SetShowGizmoIcons(showIcons);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exibir icones 3D de Luzes, Cameras, Audio e Particulas");
            ImGui::SameLine();

            // Toggle Frustum de Câmeras
            bool showFrustum = m_ActiveScene ? m_ActiveScene->GetShowCameraFrustums() : true;
            if (ImGui::Checkbox("Frustum", &showFrustum)) {
                if (m_ActiveScene) m_ActiveScene->SetShowCameraFrustums(showFrustum);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exibir frustum wireframe das cameras da cena");
            ImGui::SameLine();

            // Toggle Wireframe Colliders
            bool showColliders = m_ActiveScene ? m_ActiveScene->GetShowColliders() : true;
            if (ImGui::Checkbox("Colliders", &showColliders)) {
                if (m_ActiveScene) m_ActiveScene->SetShowColliders(showColliders);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exibir wireframe verde dos colisores 3D da entidade selecionada");
            ImGui::SameLine();

            // Flight Camera Move Speed
            float camSpeed = m_Camera.GetMoveSpeed();
            ImGui::SetNextItemWidth(50.0f);
            if (ImGui::DragFloat("##CamSpeed", &camSpeed, 0.5f, 0.5f, 50.0f, "%.1fx")) {
                m_Camera.SetMoveSpeed(camSpeed);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Velocidade de voo da câmera");
            ImGui::SameLine();

            // Exposure / HDR Tonemapping
            ImGui::SetNextItemWidth(55.0f);
            if (ImGui::DragFloat("##Exposure", &m_Exposure, 0.05f, 0.1f, 5.0f, "Exp: %.1f")) {
                if (m_ActiveScene) m_ActiveScene->SetExposure(m_Exposure);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exposição / HDR ACES Tonemapping");
            ImGui::SameLine();

            // Vignette Cinematográfico
            float vignette = m_ActiveScene ? m_ActiveScene->GetVignette() : 0.35f;
            ImGui::SetNextItemWidth(50.0f);
            if (ImGui::DragFloat("##Vignette", &vignette, 0.02f, 0.0f, 1.0f, "Vig: %.2f")) {
                if (m_ActiveScene) m_ActiveScene->SetVignette(vignette);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Vinheta cinematográfica");
            ImGui::SameLine();

            // Resolução & FPS
            ImGui::TextDisabled("%.0fx%.0f | %.0f FPS", m_ViewportSize.x, m_ViewportSize.y, m_FPS);

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f) {
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
    }

    uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID(0);
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::Image(
        reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
        ImVec2(m_ViewportSize.x, m_ViewportSize.y),
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    // Captura com máxima precisão os limites exatos da imagem na tela
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageMax = ImGui::GetItemRectMax();
    m_ViewportBounds[0] = { imageMin.x, imageMin.y };
    m_ViewportBounds[1] = { imageMax.x, imageMax.y };

    // -------------------------------------------------------------------------
    // Drag & Drop de Assets direto na Viewport (Cena, Prefabs, Modelos 3D, Texturas)
    // -------------------------------------------------------------------------
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string pathStr = (const char*)payload->Data;
            std::filesystem::path path(pathStr);
            std::string ext = path.extension().string();
            for (auto& c : ext) c = static_cast<char>(tolower(c));

            if (ext == ".ovscene" || ext == ".scene" || ext == ".overflow") {
                OpenScene(path.string());
            } else if (ext == ".ovbp" || ext == ".ovar" || ext == ".archetype" || ext == ".prefab") {
                auto entity = Overflow::SceneSerializer::DeserializeEntity(path.string(), m_ActiveScene.get());
                if (entity) {
                    m_SceneHierarchyPanel.SetSelectedEntity(entity);
                    ShowToast("Archetype instanciado no mundo: " + path.filename().string(), ImVec4(0.35f, 0.90f, 0.50f, 1.0f));
                }
            } else if (ext == ".obj") {
                auto entity = m_ActiveScene->CreateEntity(path.stem().string());
                auto& mc = entity.AddComponent<Overflow::MeshComponent>(Overflow::MeshGeometryType::CustomOBJ);
                mc.ModelAsset = Overflow::Model::Create(path.string());
                m_SceneHierarchyPanel.SetSelectedEntity(entity);
                OVERFLOW_INFO("Modelo 3D importado na cena: {0}", path.filename().string());
            } else if (ext == ".h" || ext == ".cpp") {
                Overflow::Entity targetEntity;
                if (m_HoveredEntityID > 0) {
                    targetEntity = Overflow::Entity(static_cast<uint32_t>(m_HoveredEntityID), m_ActiveScene.get());
                } else {
                    targetEntity = m_SceneHierarchyPanel.GetSelectedEntity();
                }

                if (targetEntity) {
                    std::string scriptName = path.stem().string();
                    if (!targetEntity.HasComponent<Overflow::NativeScriptComponent>()) {
                        targetEntity.AddComponent<Overflow::NativeScriptComponent>();
                    }
                    targetEntity.GetComponent<Overflow::NativeScriptComponent>().BindByName(scriptName);
                    m_SceneHierarchyPanel.SetSelectedEntity(targetEntity);
                    ShowToast("Script '" + scriptName + "' anexado a: " + targetEntity.GetComponent<Overflow::TagComponent>().Tag, ImVec4(0.35f, 0.90f, 0.50f, 1.0f), 5.0f);
                    OVERFLOW_INFO("Script '{0}' anexado a entidade '{1}' via Viewport Drag & Drop!", scriptName, targetEntity.GetComponent<Overflow::TagComponent>().Tag);
                } else {
                    ShowToast("Selecione um objeto ou solte o script diretamente sobre ele!", ImVec4(0.95f, 0.75f, 0.25f, 1.0f), 4.0f);
                }
            } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                Overflow::Entity targetEntity = (m_HoveredEntityID > 0) ? Overflow::Entity(static_cast<uint32_t>(m_HoveredEntityID), m_ActiveScene.get()) : m_SceneHierarchyPanel.GetSelectedEntity();
                if (targetEntity && targetEntity.HasComponent<Overflow::MeshComponent>()) {
                    targetEntity.GetComponent<Overflow::MeshComponent>().TextureAsset = Overflow::Texture2D::Create(path.string());
                    ShowToast("Textura aplicada a: " + targetEntity.GetComponent<Overflow::TagComponent>().Tag, ImVec4(0.35f, 0.90f, 0.50f, 1.0f));
                    OVERFLOW_INFO("Textura aplicada na entidade '{0}': {1}", targetEntity.GetComponent<Overflow::TagComponent>().Tag, path.filename().string());
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // -------------------------------------------------------------------------
    // Mouse Picking em Tempo Real na Viewport (FBO Attachment 1: R32I EntityID)
    // -------------------------------------------------------------------------
    ImVec2 mousePos = ImGui::GetMousePos();
    float mx = mousePos.x - m_ViewportBounds[0].x;
    float my = mousePos.y - m_ViewportBounds[0].y;
    glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    my = viewportSize.y - my; // Inversão do eixo Y no OpenGL

    int mouseX = static_cast<int>(mx);
    int mouseY = static_cast<int>(my);

    if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSize.x) && mouseY < static_cast<int>(viewportSize.y)) {
        int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
        m_HoveredEntityID = pixelData;

        // Se clicou com o botão esquerdo e não está usando Gizmo nem câmera:
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !m_IsCameraFlying && !m_IsCameraOrbiting && !m_IsCameraPanning) {
            if (pixelData >= 1 && m_ActiveScene->HasEntity(static_cast<uint32_t>(pixelData))) {
                m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByID(static_cast<uint32_t>(pixelData)));
            } else if (pixelData == -1) {
                m_SceneHierarchyPanel.SetSelectedEntity({});
            }
        }
    }

    // -------------------------------------------------------------------------
    // Renderização e Interação dos Gizmos 3D (ImGuizmo)
    // -------------------------------------------------------------------------
    Overflow::Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
    if (selectedEntity && m_GizmoType != -1) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        // Estilização Profissional dos Gizmos estilo Unreal Engine 5
        ImGuizmo::GetStyle().TranslationLineThickness = 4.0f;
        ImGuizmo::GetStyle().TranslationLineArrowSize = 8.0f;
        ImGuizmo::GetStyle().RotationLineThickness = 4.0f;
        ImGuizmo::GetStyle().RotationOuterLineThickness = 3.5f;
        ImGuizmo::GetStyle().ScaleLineThickness = 4.0f;
        ImGuizmo::GetStyle().ScaleLineCircleSize = 7.0f;
        ImGuizmo::GetStyle().HatchedAxisLineThickness = 6.0f;

        ImGuizmo::SetRect(windowPos.x, windowPos.y, m_ViewportSize.x, m_ViewportSize.y);

        // Matrizes da Câmera
        const glm::mat4& cameraProj = m_Camera.GetProjection();
        glm::mat4 cameraView = m_Camera.GetViewMatrix();

        // Matriz de Transformação no Mundo (World Transform)
        auto& tc = selectedEntity.GetComponent<Overflow::TransformComponent>();
        glm::mat4 transform = tc.GetWorldTransform(m_ActiveScene.get());

        // Snapping (Ativado com botão Snap ou segurando Ctrl)
        bool snap = m_GizmoSnap || Overflow::Input::IsKeyPressed(Overflow::Key::LeftControl);
        float snapValue = (m_GizmoType == 1) ? m_SnapRotation : m_SnapTranslation;
        float snapValues[3] = { snapValue, snapValue, snapValue };

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        if (m_GizmoType == 1) op = ImGuizmo::ROTATE;
        else if (m_GizmoType == 2) op = ImGuizmo::SCALE;

        ImGuizmo::MODE mode = (m_GizmoMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        float deltaMatrix[16] = { 0 };
        ImGuizmo::Manipulate(
            glm::value_ptr(cameraView),
            glm::value_ptr(cameraProj),
            op,
            mode,
            glm::value_ptr(transform),
            deltaMatrix,
            snap ? snapValues : nullptr
        );

        if (ImGuizmo::IsUsing()) {
            if (!m_GizmoWasUsing) {
                m_CurrentGizmoRecord.EntityID = selectedEntity.GetID();
                m_CurrentGizmoRecord.OldTranslation = tc.Translation;
                m_CurrentGizmoRecord.OldRotation = tc.Rotation;
                m_CurrentGizmoRecord.OldScale = tc.Scale;
                m_GizmoWasUsing = true;
            }

            if (m_GizmoType == 0) { // Translate
                if (tc.ParentID != 0 && m_ActiveScene->HasComponent<Overflow::TransformComponent>(tc.ParentID)) {
                    const auto& parentTransform = m_ActiveScene->GetComponent<Overflow::TransformComponent>(tc.ParentID);
                    glm::mat4 parentWorld = parentTransform.GetWorldTransform(m_ActiveScene.get());
                    transform = glm::inverse(parentWorld) * transform;
                }

                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(
                    glm::value_ptr(transform),
                    glm::value_ptr(translation),
                    glm::value_ptr(rotation),
                    glm::value_ptr(scale)
                );
                tc.Translation = translation;
            } else if (m_GizmoType == 1) { // Rotate (Usa deltaMatrix para evitar feedback loop de Euler / Gimbal lock)
                glm::vec3 deltaTrans, deltaRot, deltaScale;
                ImGuizmo::DecomposeMatrixToComponents(
                    deltaMatrix,
                    glm::value_ptr(deltaTrans),
                    glm::value_ptr(deltaRot),
                    glm::value_ptr(deltaScale)
                );
                tc.Rotation += deltaRot;
            } else if (m_GizmoType == 2) { // Scale
                glm::vec3 deltaTrans, deltaRot, deltaScale;
                ImGuizmo::DecomposeMatrixToComponents(
                    deltaMatrix,
                    glm::value_ptr(deltaTrans),
                    glm::value_ptr(deltaRot),
                    glm::value_ptr(deltaScale)
                );
                tc.Scale *= deltaScale;
            }
        } else if (m_GizmoWasUsing) {
            // Finalizou o arraste com o Gizmo: registra o comando na pilha de Undo (Ctrl+Z)
            m_CurrentGizmoRecord.NewTranslation = tc.Translation;
            m_CurrentGizmoRecord.NewRotation = tc.Rotation;
            m_CurrentGizmoRecord.NewScale = tc.Scale;
            m_UndoStack.push_back(m_CurrentGizmoRecord);
            m_RedoStack.clear();
            m_GizmoWasUsing = false;
        }
    }

    // -------------------------------------------------------------------------
    // ViewCube / Orientation Gizmo 3D Interativo (Estilo Maya / Blender / Unreal Engine 5)
    // -------------------------------------------------------------------------
    float viewCubeSize = 95.0f;
    ImVec2 viewCubePos = ImVec2(
        m_ViewportBounds[1].x - viewCubeSize - 16.0f,
        m_ViewportBounds[0].y + 75.0f
    );

    glm::mat4 manipView = m_Camera.GetViewMatrix();
    float camDist = glm::length(m_Camera.GetPosition());
    if (camDist < 0.1f) camDist = 5.0f;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::ViewManipulate(
        glm::value_ptr(manipView),
        camDist,
        viewCubePos,
        ImVec2(viewCubeSize, viewCubeSize),
        0x10101010
    );

    if (ImGuizmo::IsUsingViewManipulate()) {
        glm::mat4 invView = glm::inverse(manipView);
        glm::vec3 newPos = glm::vec3(invView[3]);
        glm::vec3 newForward = -glm::vec3(invView[2]);
        m_Camera.SetPosition(newPos);
        float pitch = glm::degrees(asin(std::clamp(newForward.y, -0.999f, 0.999f)));
        float yaw = glm::degrees(atan2(newForward.z, newForward.x));
        m_Camera.SetRotation(pitch, yaw);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::RenderGamePanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Live Simulation", &m_ShowGameView);

    m_GameViewportFocused = ImGui::IsWindowFocused();
    m_GameViewportHovered = ImGui::IsWindowHovered();

    // -------------------------------------------------------------------------
    // Toolbar Superior da Janela Game (Estilo Unity 6 Game View Toolbar)
    // -------------------------------------------------------------------------
    float toolbarHeight = 32.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.125f, 0.14f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));

    if (ImGui::BeginChild("##GameViewToolbar", ImVec2(0.0f, toolbarHeight), false, ImGuiWindowFlags_NoScrollbar)) {
        // Dropdown de Aspect Ratio (Unity style)
        ImGui::SetNextItemWidth(140.0f);
        const char* aspectRatios[] = {
            "Free Aspect",
            "16:9 (1920x1080)",
            "16:10",
            "4:3",
            "21:9 Ultrawide",
            "1:1 Square"
        };
        ImGui::Combo("##AspectRatio", &m_GameAspectRatio, aspectRatios, IM_ARRAYSIZE(aspectRatios));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Proporcao de tela (Aspect Ratio)");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("##GameScale", &m_GameScale, 0.5f, 2.0f, "Scale: %.1fx");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Escala de exibicao");

        ImGui::SameLine();
        ImGui::Checkbox("Maximize on Play", &m_MaximizeOnPlay);

        ImGui::SameLine();
        ImGui::Checkbox("Stats", &m_ShowGameStats);

        ImGui::SameLine();
        ImGui::Checkbox("HUD", &m_ShowGameHUD);

        ImGui::SameLine();
        if (m_MuteAudio) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
            if (ImGui::Button("Muted [M]")) m_MuteAudio = false;
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Audio [M]")) m_MuteAudio = true;
        }

        // Lado Direito: Nome da Câmera Ativa
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 220.0f);
        bool hasCam = m_ActiveScene && m_ActiveScene->HasActiveCamera();
        if (hasCam) {
            auto camEntity = m_ActiveScene->GetPrimaryCameraEntity();
            std::string camName = camEntity ? camEntity.GetComponent<Overflow::TagComponent>().Tag : "Main Camera";
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 0.95f, 1.0f), "Cam: %s", camName.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.30f, 1.0f), "No Active Camera!");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // -------------------------------------------------------------------------
    // Renderização da Imagem da Câmera com Letterboxing/Pillarboxing
    // -------------------------------------------------------------------------
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0.0f && avail.y > 0.0f) {
        float targetW = avail.x;
        float targetH = avail.y;

        float aspect = 0.0f;
        if (m_GameAspectRatio == 1) aspect = 16.0f / 9.0f;
        else if (m_GameAspectRatio == 2) aspect = 16.0f / 10.0f;
        else if (m_GameAspectRatio == 3) aspect = 4.0f / 3.0f;
        else if (m_GameAspectRatio == 4) aspect = 21.0f / 9.0f;
        else if (m_GameAspectRatio == 5) aspect = 1.0f;

        if (aspect > 0.0f) {
            if (avail.x / avail.y > aspect) {
                targetH = avail.y;
                targetW = targetH * aspect;
            } else {
                targetW = avail.x;
                targetH = targetW / aspect;
            }
        }

        targetW *= m_GameScale;
        targetH *= m_GameScale;

        m_GameViewportSize = { targetW, targetH };

        // Centraliza a imagem no viewport
        float offsetX = (avail.x - targetW) * 0.5f;
        float offsetY = (avail.y - targetH) * 0.5f;
        if (offsetX < 0.0f) offsetX = 0.0f;
        if (offsetY < 0.0f) offsetY = 0.0f;

        bool hasCam = m_ActiveScene && m_ActiveScene->HasActiveCamera();

        if (hasCam && m_GameFramebuffer) {
            uint32_t textureID = m_GameFramebuffer->GetColorAttachmentRendererID(0);
            ImGui::SetCursorPos(ImVec2(offsetX, toolbarHeight + offsetY));
            ImGui::Image(
                reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
                ImVec2(targetW, targetH),
                ImVec2(0, 1),
                ImVec2(1, 0)
            );

            // -----------------------------------------------------------------
            // Overlay de UI / Canvas de Jogo (Crosshair, Barra de Vida, HUD)
            // -----------------------------------------------------------------
            if (m_ShowGameHUD) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 imgMin = ImGui::GetItemRectMin();
                ImVec2 imgMax = ImGui::GetItemRectMax();
                ImVec2 center = ImVec2((imgMin.x + imgMax.x) * 0.5f, (imgMin.y + imgMax.y) * 0.5f);

                // 1. Crosshair Central (Mira de Precisão Estilo AAA)
                float chSize = 9.0f;
                float chGap = 3.0f;
                ImU32 chColor = IM_COL32(230, 240, 255, 200);
                drawList->AddLine(ImVec2(center.x - chSize, center.y), ImVec2(center.x - chGap, center.y), chColor, 1.5f);
                drawList->AddLine(ImVec2(center.x + chGap, center.y), ImVec2(center.x + chSize, center.y), chColor, 1.5f);
                drawList->AddLine(ImVec2(center.x, center.y - chSize), ImVec2(center.x, center.y - chGap), chColor, 1.5f);
                drawList->AddLine(ImVec2(center.x, center.y + chGap), ImVec2(center.x, center.y + chSize), chColor, 1.5f);
                drawList->AddCircleFilled(center, 1.5f, IM_COL32(255, 255, 255, 240));

                // 2. Barra de Vida no Canto Superior Esquerdo
                ImVec2 barPos = ImVec2(imgMin.x + 20.0f, imgMin.y + 20.0f);
                float barW = 160.0f;
                float barH = 14.0f;
                drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH), IM_COL32(30, 30, 35, 220), 4.0f);
                drawList->AddRectFilled(barPos, ImVec2(barPos.x + barW * 0.85f, barPos.y + barH), IM_COL32(65, 215, 95, 240), 4.0f);
                drawList->AddRect(barPos, ImVec2(barPos.x + barW, barPos.y + barH), IM_COL32(180, 200, 220, 160), 4.0f, 0, 1.0f);
                drawList->AddText(ImVec2(barPos.x + 8.0f, barPos.y - 1.0f), IM_COL32(255, 255, 255, 240), "HP 85 / 100");

                // 3. Instruções de Controle Flutuantes no Rodapé
                std::string controlHelp = (m_SceneState == SceneState::Play) ? "Running: W/A/S/D Move | Space Jump" : "Press Play [>] to Simulate Gameplay";
                drawList->AddText(ImVec2(imgMin.x + 20.0f, imgMax.y - 28.0f), IM_COL32(220, 230, 240, 190), controlHelp.c_str());
            }
        } else {
            // Mensagem elegante estilo Unity quando nenhuma câmera está presente
            float textY = toolbarHeight + (avail.y * 0.40f);
            ImGui::SetCursorPos(ImVec2(avail.x * 0.5f - 160.0f, textY));
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "No cameras are rendering in this scene.");
            ImGui::SetCursorPos(ImVec2(avail.x * 0.5f - 210.0f, textY + 24.0f));
            ImGui::TextDisabled("Add a Camera Component to an entity or click [+ Add] -> Camera.");
        }

        // ---------------------------------------------------------------------
        // Overlay Flutuante de Estatísticas (Stats Window estilo Unity)
        // ---------------------------------------------------------------------
        if (m_ShowGameStats) {
            float statsW = 220.0f;
            float statsH = 110.0f;
            ImGui::SetCursorPos(ImVec2(avail.x - statsW - 14.0f, toolbarHeight + 14.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.085f, 0.10f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.26f, 0.7f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

            if (ImGui::BeginChild("##GameStatsOverlay", ImVec2(statsW, statsH), true)) {
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 0.95f, 1.0f), "Game View Statistics");
                ImGui::Separator();
                ImGui::Text("FPS: %.1f (%.2f ms)", m_FPS, m_LastFrameTimeMs);
                ImGui::Text("Resolution: %.0f x %.0f", targetW, targetH);
                ImGui::Text("Entities: %zu", m_ActiveScene ? m_ActiveScene->GetEntities().size() : 0);
                ImGui::TextDisabled("Backend: OpenGL 4.6 Core");
            }
            ImGui::EndChild();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::RenderMetricsPanel() {
    ImGui::Begin("Performance & Metrics", &m_ShowMetrics);

    ImGui::Text("FPS: %.1f  (Frame Time: %.2f ms)", m_FPS, m_LastFrameTimeMs);
    ImGui::PlotLines("##FPSHistory", m_FPSHistory, 60, m_FPSHistoryIndex, nullptr, 0.0f, 240.0f, ImVec2(-1.0f, 45.0f));

    ImGui::Separator();
    ImGui::Text("Viewport: %.0f x %.0f", m_ViewportSize.x, m_ViewportSize.y);
    ImGui::Text("Entities in Scene: %zu", m_ActiveScene->GetEntities().size());

    // Breakdown de componentes na cena (estilo Unity Profiler)
    size_t meshCount = 0;
    size_t lightCount = 0;
    size_t cameraCount = 0;
    for (uint32_t id : m_ActiveScene->GetEntities()) {
        if (m_ActiveScene->HasComponent<Overflow::MeshComponent>(id)) meshCount++;
        if (m_ActiveScene->HasComponent<Overflow::LightComponent>(id)) lightCount++;
        if (m_ActiveScene->HasComponent<Overflow::CameraComponent>(id)) cameraCount++;
    }
    ImGui::BulletText("Meshes 3D: %zu", meshCount);
    ImGui::BulletText("Fontes de Luz: %zu", lightCount);
    ImGui::BulletText("Câmeras: %zu", cameraCount);

    ImGui::Separator();
    if (m_HoveredEntityID >= 1) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "Hovered Entity ID: %d", m_HoveredEntityID);
    } else {
        ImGui::TextDisabled("Hovered Entity ID: None (-1)");
    }

    const auto& camPos = m_Camera.GetPosition();
    ImGui::Text("Camera Pos: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
    ImGui::Text("Simulation State: %s", (m_SceneState == SceneState::Play) ? "PLAYING" : ((m_SceneState == SceneState::Pause) ? "PAUSED" : "EDIT"));

    ImGui::Separator();
    ImGui::ColorEdit4("Clear Color", glm::value_ptr(m_ClearColor));

    ImGui::End();
}

void EditorLayer::RenderAboutModal() {
    ImGui::OpenPopup("About Overflow Engine");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 260.0f));

    if (ImGui::BeginPopupModal("About Overflow Engine", &m_ShowAboutModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.35f, 0.55f, 0.90f, 1.0f), "Overflow Engine 3D");
        ImGui::Text("Professional C++20 / OpenGL 4.6 Core Profile Game Engine");
        ImGui::Separator();

        ImGui::BulletText("Renderer: Cook-Torrance GGX PBR + PCF Soft Shadows + Infinite Grid");
        ImGui::BulletText("Editor: Docking Layout, ImGuizmo, Scene Graph, MRT GPU Mouse Picking");
        ImGui::BulletText("Workflow: Content Browser, Dockable Console, Scene Serializer");
        ImGui::BulletText("Simulation: Runtime Play / Pause / Stop State Machine");

        ImGui::Separator();
        if (ImGui::Button("Fechar", ImVec2(120.0f, 0.0f))) {
            m_ShowAboutModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::RenderCreateScriptModal() {
    ImGui::OpenPopup("Create C++ Script");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 220.0f));

    if (ImGui::BeginPopupModal("Create C++ Script", &m_ShowCreateScriptModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Novo Script C++ (Estilo Unity / Unreal)");
        ImGui::TextDisabled("Herda de ScriptableEntity com OnCreate() e OnUpdate(ts).");
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::Text("Nome da Classe C++:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##NewScriptClassName", m_NewScriptClassName, sizeof(m_NewScriptClassName));

        ImGui::Spacing();
        ImGui::TextDisabled("Sera criado em: src/Engine/Scene/Scripts/%s.h", m_NewScriptClassName);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Create Script", ImVec2(130.0f, 28.0f))) {
            std::string outPath;
            if (Overflow::ScriptManager::CreateScript(m_NewScriptClassName, outPath)) {
                ShowToast("Script C++ criado: " + std::string(m_NewScriptClassName) + ".h", ImVec4(0.35f, 0.90f, 0.50f, 1.0f), 5.0f);
                m_ShowCreateScriptModal = false;
                ImGui::CloseCurrentPopup();
            } else {
                ShowToast("Erro ao criar script ou arquivo ja existente!", ImVec4(0.95f, 0.35f, 0.25f, 1.0f));
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Create & Open in Rider", ImVec2(180.0f, 28.0f))) {
            std::string outPath;
            if (Overflow::ScriptManager::CreateScript(m_NewScriptClassName, outPath)) {
                ShowToast("Script criado! Abrindo no Rider...", ImVec4(0.35f, 0.90f, 0.50f, 1.0f), 5.0f);
                Overflow::ScriptManager::OpenInRider();
                m_ShowCreateScriptModal = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(80.0f, 28.0f))) {
            m_ShowCreateScriptModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::RenderPreferencesModal() {
    ImGui::OpenPopup("Engine Preferences & External Tools");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 380.0f));

    if (ImGui::BeginPopupModal("Engine Preferences & External Tools", &m_ShowPreferencesModal)) {
        ImGui::TextColored(ImVec4(0.35f, 0.70f, 1.0f, 1.0f), "Configuracoes da Overflow Engine");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTabBar("PreferencesTabs")) {
            if (ImGui::BeginTabItem("External Code Editor")) {
                ImGui::Spacing();
                ImGui::Text("Editor de Codigo / IDE Padrao para Scripts e Shaders:");
                ImGui::Spacing();

                auto currentIDE = Overflow::EditorPreferences::GetPreferredIDE();
                int selectedChoice = static_cast<int>(currentIDE);

                const char* ideOptions[] = {
                    "JetBrains Rider (Recomendado para C++ / Unreal / Unity)",
                    "Microsoft Visual Studio (2022 / Community / Pro)",
                    "Visual Studio Code",
                    "System Default (Associacao Padrao do Windows)",
                    "Custom Executable Path"
                };

                for (int i = 0; i < 5; i++) {
                    if (ImGui::RadioButton(ideOptions[i], selectedChoice == i)) {
                        selectedChoice = i;
                        Overflow::EditorPreferences::SetPreferredIDE(static_cast<Overflow::CodeEditorChoice>(i));
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (selectedChoice == static_cast<int>(Overflow::CodeEditorChoice::Custom)) {
                    ImGui::Text("Caminho do Executavel Customizado:");
                    static char s_CustomPath[512] = "";
                    if (s_CustomPath[0] == '\0') {
                        strncpy_s(s_CustomPath, Overflow::EditorPreferences::GetCustomIDEPath().c_str(), sizeof(s_CustomPath));
                    }
                    if (ImGui::InputText("##CustomIDEPath", s_CustomPath, sizeof(s_CustomPath))) {
                        Overflow::EditorPreferences::SetCustomIDEPath(s_CustomPath);
                    }
                } else {
                    ImGui::TextDisabled("IDE Ativa: %s", Overflow::EditorPreferences::GetIDEName(Overflow::EditorPreferences::GetPreferredIDE()));
                    ImGui::TextDisabled("Ao dar duplo clique em scripts C++ (.h/.cpp), shaders (.glsl) ou materiais (.ovmat),");
                    ImGui::TextDisabled("o arquivo sera aberto instantaneamente na IDE selecionada.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Test Launch Preferred Editor", ImVec2(220.0f, 26.0f))) {
                    Overflow::EditorPreferences::OpenFileInIDE("CMakeLists.txt");
                    ShowToast("Disparando abertura da IDE configurada...", ImVec4(0.35f, 0.85f, 0.5f, 1.0f));
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Content Browser & Engine")) {
                ImGui::Spacing();
                ImGui::Text("Diretorio Base de Assets: assets/");
                ImGui::Text("Formatos Nativos Suportados:");
                ImGui::BulletText("Scripts: .h, .cpp (EntityBehavior C++)");
                ImGui::BulletText("Shaders: .glsl, .vert, .frag (OpenGL Core 4.6)");
                ImGui::BulletText("Arquetipos: .ovbp (Overflow Blueprints)");
                ImGui::BulletText("Materiais: .ovmat (PBR Surface Properties)");
                ImGui::BulletText("Cenas: .ovscene (Mundos e Ambientes 3D)");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save & Close", ImVec2(120.0f, 28.0f))) {
            Overflow::EditorPreferences::Save();
            m_ShowPreferencesModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorLayer::FocusSelectedEntity() {
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    if (selected && selected.HasComponent<Overflow::TransformComponent>()) {
        const auto& tc = selected.GetComponent<Overflow::TransformComponent>();
        glm::mat4 world = tc.GetWorldTransform(m_ActiveScene.get());
        glm::vec3 worldPos = glm::vec3(world[3]);
        float size = glm::length(tc.Scale);
        float dist = std::clamp(size * 2.8f, 2.5f, 50.0f);
        m_Camera.Focus(worldPos, dist);
        std::string name = selected.GetComponent<Overflow::TagComponent>().Tag;
        ShowToast("Camera focada em: " + name, ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
    } else {
        // Se nenhuma entidade estiver selecionada, foca no centro da cena (0, 0, 0)
        m_Camera.Focus(glm::vec3(0.0f, 0.0f, 0.0f), 8.0f);
        ShowToast("Camera centralizada na cena (Origem)", ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
    }
}

void EditorLayer::DuplicateSelectedEntity() {
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    if (selected) {
        auto copy = m_ActiveScene->CreateEntity(selected.GetComponent<Overflow::TagComponent>().Tag + " (Copy)");
        copy.GetComponent<Overflow::TransformComponent>() = selected.GetComponent<Overflow::TransformComponent>();
        copy.GetComponent<Overflow::TransformComponent>().Translation += glm::vec3(0.5f, 0.0f, 0.5f);
        if (selected.HasComponent<Overflow::MeshComponent>()) copy.AddComponent<Overflow::MeshComponent>() = selected.GetComponent<Overflow::MeshComponent>();
        if (selected.HasComponent<Overflow::LightComponent>()) copy.AddComponent<Overflow::LightComponent>() = selected.GetComponent<Overflow::LightComponent>();
        if (selected.HasComponent<Overflow::CameraComponent>()) copy.AddComponent<Overflow::CameraComponent>() = selected.GetComponent<Overflow::CameraComponent>();
        m_SceneHierarchyPanel.SetSelectedEntity(copy);
        ShowToast("Entidade duplicada com sucesso!", ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
    }
}

void EditorLayer::DeleteSelectedEntity() {
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    if (selected) {
        std::string name = selected.GetComponent<Overflow::TagComponent>().Tag;
        m_ActiveScene->DestroyEntity(selected);
        m_SceneHierarchyPanel.SetSelectedEntity({});
        ShowToast("Entidade removida: " + name, ImVec4(0.9f, 0.4f, 0.3f, 1.0f));
    }
}

void EditorLayer::Undo() {
    if (m_UndoStack.empty()) {
        ShowToast("Nenhuma acao para desfazer (Undo)", ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        return;
    }
    auto record = m_UndoStack.back();
    m_UndoStack.pop_back();

    if (m_ActiveScene && m_ActiveScene->HasEntity(record.EntityID)) {
        auto entity = m_ActiveScene->GetEntityByID(record.EntityID);
        if (entity.HasComponent<Overflow::TransformComponent>()) {
            auto& tc = entity.GetComponent<Overflow::TransformComponent>();
            tc.Translation = record.OldTranslation;
            tc.Rotation = record.OldRotation;
            tc.Scale = record.OldScale;
            m_RedoStack.push_back(record);
            ShowToast("Desfeito: Transform revertido (Ctrl+Z)", ImVec4(0.35f, 0.85f, 0.95f, 1.0f));
        }
    }
}

void EditorLayer::Redo() {
    if (m_RedoStack.empty()) {
        ShowToast("Nenhuma acao para refazer (Redo)", ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        return;
    }
    auto record = m_RedoStack.back();
    m_RedoStack.pop_back();

    if (m_ActiveScene && m_ActiveScene->HasEntity(record.EntityID)) {
        auto entity = m_ActiveScene->GetEntityByID(record.EntityID);
        if (entity.HasComponent<Overflow::TransformComponent>()) {
            auto& tc = entity.GetComponent<Overflow::TransformComponent>();
            tc.Translation = record.NewTranslation;
            tc.Rotation = record.NewRotation;
            tc.Scale = record.NewScale;
            m_UndoStack.push_back(record);
            ShowToast("Refeito: Transform reaplicado (Ctrl+Y)", ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
        }
    }
}

void EditorLayer::ShowToast(const std::string& message, const ImVec4& color, float duration) {
    m_Toasts.push_back({ message, duration, color });
    if (m_Toasts.size() > 5) {
        m_Toasts.erase(m_Toasts.begin());
    }
}

void EditorLayer::RenderStatusBar() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float statusBarHeight = 26.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration 
                           | ImGuiWindowFlags_NoDocking 
                           | ImGuiWindowFlags_NoMove 
                           | ImGuiWindowFlags_NoResize 
                           | ImGuiWindowFlags_NoScrollbar 
                           | ImGuiWindowFlags_NoSavedSettings 
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.095f, 0.11f, 1.0f));

    if (ImGui::Begin("##MainStatusBar", nullptr, flags)) {
        // Lado Esquerdo: Atalhos rápidos e indicador de simulação
        if (ImGui::Button("[Content]")) {
            m_ShowContentBrowser = !m_ShowContentBrowser;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alternar visibilidade do Content Browser");
        ImGui::SameLine();

        if (ImGui::Button("[Output]")) {
            m_ShowConsole = !m_ShowConsole;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alternar visibilidade do Console");
        ImGui::SameLine();

        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (m_SceneState == SceneState::Play) {
            ImGui::TextColored(ImVec4(0.25f, 0.85f, 0.40f, 1.0f), "Simulating (Play)");
        } else if (m_SceneState == SceneState::Pause) {
            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Paused");
        } else {
            ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.0f, 1.0f), "Ready (Edit)");
        }

        // Centro: Telemetria da entidade selecionada
        ImGui::SameLine(viewport->WorkSize.x * 0.32f);
        auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
        if (selected) {
            const auto& tag = selected.GetComponent<Overflow::TagComponent>().Tag;
            const auto& tc = selected.GetComponent<Overflow::TransformComponent>();
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "Selected: %s (ID: %u)  Pos: (%.1f, %.1f, %.1f)", 
                tag.c_str(), selected.GetID(), tc.Translation.x, tc.Translation.y, tc.Translation.z);
        } else {
            ImGui::TextDisabled("No entity selected");
        }

        // Lado Direito: Notificação ativa e telemetria
        float rightStatsPos = viewport->WorkSize.x - 350.0f;
        if (rightStatsPos > viewport->WorkSize.x * 0.60f) {
            ImGui::SameLine(rightStatsPos);
            if (!m_Toasts.empty()) {
                const auto& toast = m_Toasts.back();
                ImGui::TextColored(toast.Color, "[i] %s", toast.Message.c_str());
            } else {
                ImGui::TextDisabled("%.0fx%.0f | %.0f FPS | OpenGL 4.6", m_ViewportSize.x, m_ViewportSize.y, m_FPS);
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void EditorLayer::RenderToastNotifications() {
    if (m_Toasts.empty()) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float toastWidth = 340.0f;
    float startY = viewport->WorkPos.y + viewport->WorkSize.y - 32.0f;

    for (int i = static_cast<int>(m_Toasts.size()) - 1; i >= 0; --i) {
        const auto& toast = m_Toasts[i];
        float alpha = std::clamp(toast.RemainingSeconds / 0.5f, 0.0f, 1.0f);
        if (toast.RemainingSeconds > 3.0f) {
            alpha = std::clamp((3.5f - toast.RemainingSeconds) / 0.5f, 0.0f, 1.0f);
        }

        startY -= 38.0f;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - toastWidth - 16.0f, startY));
        ImGui::SetNextWindowSize(ImVec2(toastWidth, 32.0f));
        ImGui::SetNextWindowBgAlpha(0.92f * alpha);

        ImGuiWindowFlags toastFlags = ImGuiWindowFlags_NoDecoration 
                                    | ImGuiWindowFlags_NoDocking 
                                    | ImGuiWindowFlags_NoMove 
                                    | ImGuiWindowFlags_NoResize 
                                    | ImGuiWindowFlags_NoSavedSettings 
                                    | ImGuiWindowFlags_NoFocusOnAppearing 
                                    | ImGuiWindowFlags_NoNav;

        std::string windowId = "##Toast_" + std::to_string(i);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(toast.Color.x, toast.Color.y, toast.Color.z, 0.6f * alpha));

        if (ImGui::Begin(windowId.c_str(), nullptr, toastFlags)) {
            ImGui::TextColored(ImVec4(toast.Color.x, toast.Color.y, toast.Color.z, alpha), "[i] %s", toast.Message.c_str());
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}
