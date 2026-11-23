#include "SceneHierarchyPanel.h"
#include "Engine/Scene/Component.h"
#include "Engine/Scene/Scripts/GameplayScripts.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"
#include "EditorPreferences.h"
#include <fstream>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Overflow {

    static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 85.0f) {
        ImGui::PushID(label.c_str());

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

        float lineHeight = ImGui::GetFrameHeight();
        ImVec2 buttonSize = { 18.0f, lineHeight };

        // X Control (Red)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("X", buttonSize)) values.x = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##X", &values.x, 0.05f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Y Control (Green)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.80f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
        if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &values.y, 0.05f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Z Control (Blue)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.30f, 0.70f, 1.0f));
        if (ImGui::Button("Z", buttonSize)) values.z = resetValue;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &values.z, 0.05f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();
    }

    SceneHierarchyPanel::SceneHierarchyPanel(const std::shared_ptr<Scene>& scene) {
        RegisterEngineScripts();
        SetContext(scene);
    }

    void SceneHierarchyPanel::SetContext(const std::shared_ptr<Scene>& scene) {
        m_Context = scene;
        m_SelectionContext = {};
        m_SelectedAsset.clear();
    }

    void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
        m_SelectionContext = entity;
        if (entity) {
            m_SelectedAsset.clear();
        }
    }

    void SceneHierarchyPanel::SetSelectedAsset(const std::filesystem::path& assetPath) {
        m_SelectedAsset = assetPath;
        m_SelectionContext = {};
    }

    void SceneHierarchyPanel::ClearSelection() {
        m_SelectionContext = {};
        m_SelectedAsset.clear();
    }

    void SceneHierarchyPanel::DrawAssetInspector(const std::filesystem::path& assetPath) {
        if (!std::filesystem::exists(assetPath)) {
            ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f), "Asset nao encontrado: %s", assetPath.string().c_str());
            return;
        }

        std::string filename = assetPath.filename().string();
        std::string ext = assetPath.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));
        uintmax_t fileSize = std::filesystem::file_size(assetPath);

        // ---------------------------------------------------------------------
        // 1. MATERIAL ASSET (.ovmat, .mat)
        // ---------------------------------------------------------------------
        if (ext == ".ovmat" || ext == ".mat") {
            if (m_InspectedMaterialPath != assetPath.string() || !m_InspectedMaterial) {
                m_InspectedMaterial = Material::Load(assetPath.string());
                m_InspectedMaterialPath = assetPath.string();
            }

            if (!m_InspectedMaterial) {
                ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f), "Erro ao carregar o arquivo de material .ovmat!");
                return;
            }

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.35f, 0.30f, 0.9f));
            if (ImGui::CollapsingHeader("[Material Asset] Propriedades PBR da Superficie", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.70f, 1.0f), "Material: %s", m_InspectedMaterial->Name.c_str());
                ImGui::TextDisabled("Arquivo: %s", assetPath.string().c_str());
                ImGui::Separator();
                ImGui::Spacing();

                // Tipo de Shader do Material
                const char* shaderTypes[] = { "Standard PBR (Cook-Torrance)", "Unlit (Flat/Color)", "Blinn-Phong (Specular)", "Wireframe" };
                int currentType = static_cast<int>(m_InspectedMaterial->Type);
                if (ImGui::Combo("Shader Type", &currentType, shaderTypes, IM_ARRAYSIZE(shaderTypes))) {
                    m_InspectedMaterial->Type = static_cast<ShaderType>(currentType);
                }
                ImGui::Spacing();

                // Albedo Color & Albedo Texture
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f), "Albedo & Superficie");
                ImGui::ColorEdit4("Albedo Color", glm::value_ptr(m_InspectedMaterial->AlbedoColor));

                std::string texLabel = m_InspectedMaterial->AlbedoTexturePath.empty() ? "[Arraste uma textura aqui]" : m_InspectedMaterial->AlbedoTexturePath;
                ImGui::Button(texLabel.c_str(), ImVec2(-60.0f, 0.0f));
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string droppedPath = (const char*)payload->Data;
                        std::string dExt = std::filesystem::path(droppedPath).extension().string();
                        for (auto& c : dExt) c = static_cast<char>(tolower(c));
                        if (dExt == ".png" || dExt == ".jpg" || dExt == ".jpeg" || dExt == ".bmp") {
                            m_InspectedMaterial->AlbedoTexturePath = droppedPath;
                            m_InspectedMaterial->AlbedoTexture = Texture2D::Create(droppedPath);
                            OVERFLOW_INFO("Textura '{0}' vinculada ao material '{1}'!", droppedPath, m_InspectedMaterial->Name);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear##Albedo")) {
                    m_InspectedMaterial->AlbedoTexturePath = "";
                    m_InspectedMaterial->AlbedoTexture = nullptr;
                }

                ImGui::SliderFloat("Metallic", &m_InspectedMaterial->Metallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Roughness", &m_InspectedMaterial->Roughness, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("AO (Ambient Occlusion)", &m_InspectedMaterial->AO, 0.0f, 1.0f, "%.2f");
                ImGui::DragFloat("Tiling", &m_InspectedMaterial->Tiling, 0.1f, 0.1f, 50.0f, "%.1fx");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Normal Map
                ImGui::TextColored(ImVec4(0.45f, 0.75f, 0.95f, 1.0f), "Relevo & Normal Map");
                ImGui::Checkbox("Usar Normal Map", &m_InspectedMaterial->UseNormalMap);
                if (m_InspectedMaterial->UseNormalMap) {
                    std::string normLabel = m_InspectedMaterial->NormalMapPath.empty() ? "[Arraste o mapa de normais aqui]" : m_InspectedMaterial->NormalMapPath;
                    ImGui::Button(normLabel.c_str(), ImVec2(-60.0f, 0.0f));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::string droppedPath = (const char*)payload->Data;
                            m_InspectedMaterial->NormalMapPath = droppedPath;
                            m_InspectedMaterial->NormalMap = Texture2D::Create(droppedPath);
                            OVERFLOW_INFO("Normal map '{0}' vinculado ao material!", droppedPath);
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear##Norm")) {
                        m_InspectedMaterial->NormalMapPath = "";
                        m_InspectedMaterial->NormalMap = nullptr;
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Emissao
                ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.30f, 1.0f), "Emissao de Luz (Glow)");
                ImGui::ColorEdit3("Emission Color", glm::value_ptr(m_InspectedMaterial->EmissionColor));
                ImGui::DragFloat("Emission Intensity", &m_InspectedMaterial->EmissionIntensity, 0.1f, 0.0f, 30.0f, "%.2f");

                if (m_InspectedMaterial->Type == ShaderType::BlinnPhong) {
                    ImGui::Spacing();
                    ImGui::SliderFloat("Shininess", &m_InspectedMaterial->Shininess, 1.0f, 256.0f, "%.0f");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Ações de Salvamento do Material
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.45f, 1.0f));
                if (ImGui::Button("Salvar Alteracoes no Material (.ovmat)", ImVec2(-1.0f, 32.0f))) {
                    if (m_InspectedMaterial->Save(assetPath.string())) {
                        OVERFLOW_INFO("Material '{0}' salvo com sucesso em '{1}'!", m_InspectedMaterial->Name, assetPath.string());
                    }
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 2. MODELO 3D (.obj, .fbx, .gltf, .dae)
        // ---------------------------------------------------------------------
        else if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".dae") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.25f, 0.45f, 0.9f));
            if (ImGui::CollapsingHeader("[3D Model Asset] Informacoes de Geometria", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.85f, 0.65f, 1.0f, 1.0f), "Modelo 3D: %s", filename.c_str());
                ImGui::TextDisabled("Caminho: %s", assetPath.string().c_str());
                ImGui::TextDisabled("Tamanho no Disco: %.2f KB", fileSize / 1024.0f);
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Formato suportado nativamente pelo carregador de malhas da engine.");
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.50f, 0.85f, 1.0f));
                if (ImGui::Button("Instanciar Modelo na Cena Atual", ImVec2(-1.0f, 32.0f))) {
                    if (m_Context) {
                        std::string entityName = assetPath.stem().string();
                        Entity newEntity = m_Context->CreateEntity(entityName);
                        auto& mesh = newEntity.AddComponent<MeshComponent>(MeshGeometryType::CustomOBJ);
                        mesh.ModelAsset = Model::Create(assetPath.string());
                        mesh.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                        SetSelectedEntity(newEntity);
                        OVERFLOW_INFO("Modelo '{0}' instanciado na cena com sucesso!", assetPath.string());
                    }
                }
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 3. SCRIPTS (.ovsc, .h, .cpp, .cs, .lua)
        // ---------------------------------------------------------------------
        else if (ext == ".h" || ext == ".cpp" || ext == ".cs" || ext == ".lua" || ext == ".ovsc") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.35f, 0.55f, 0.9f));
            if (ImGui::CollapsingHeader("[Script Asset] Codigo Fonte & Comportamento", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "Script: %s", filename.c_str());
                ImGui::TextDisabled("Caminho: %s", assetPath.string().c_str());
                ImGui::TextDisabled("Tamanho: %llu bytes", fileSize);
                ImGui::Separator();
                ImGui::Spacing();

                // Botão de abrir na IDE configurada
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.25f, 0.65f, 1.0f));
                if (ImGui::Button("Abrir no Editor Externo / IDE (Rider / VS)", ImVec2(-1.0f, 28.0f))) {
                    EditorPreferences::OpenFileInIDE(assetPath);
                }
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Visualizacao Previa do Codigo:");

                // Leitura e cache do arquivo
                if (m_CachedFilePath != assetPath.string()) {
                    std::ifstream fin(assetPath);
                    if (fin.is_open()) {
                        std::stringstream ss;
                        ss << fin.rdbuf();
                        m_CachedFileContent = ss.str();
                        m_CachedFilePath = assetPath.string();
                    } else {
                        m_CachedFileContent = "// Nao foi possivel abrir o arquivo para leitura.";
                    }
                }

                ImGui::BeginChild("ScriptCodePreview", ImVec2(0.0f, 320.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 0.85f, 1.0f));
                ImGui::TextUnformatted(m_CachedFileContent.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 4. SHADERS (.glsl, .vert, .frag)
        // ---------------------------------------------------------------------
        else if (ext == ".glsl" || ext == ".vert" || ext == ".frag") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.45f, 0.35f, 0.15f, 0.9f));
            if (ImGui::CollapsingHeader("[Shader GLSL Asset] Codigo de Shader", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "Shader: %s", filename.c_str());
                ImGui::TextDisabled("Caminho: %s", assetPath.string().c_str());
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.35f, 0.15f, 1.0f));
                if (ImGui::Button("Editar Shader na IDE Externa", ImVec2(-1.0f, 28.0f))) {
                    EditorPreferences::OpenFileInIDE(assetPath);
                }
                ImGui::PopStyleColor();

                if (m_CachedFilePath != assetPath.string()) {
                    std::ifstream fin(assetPath);
                    if (fin.is_open()) {
                        std::stringstream ss;
                        ss << fin.rdbuf();
                        m_CachedFileContent = ss.str();
                        m_CachedFilePath = assetPath.string();
                    }
                }

                ImGui::BeginChild("ShaderCodePreview", ImVec2(0.0f, 320.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.90f, 0.70f, 1.0f));
                ImGui::TextUnformatted(m_CachedFileContent.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 5. TEXTURAS (.png, .jpg, .jpeg, .bmp, .tga)
        // ---------------------------------------------------------------------
        else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.40f, 0.45f, 0.9f));
            if (ImGui::CollapsingHeader("[Texture 2D Asset] Pre-visualizacao de Imagem", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.95f, 1.0f), "Textura: %s", filename.c_str());
                ImGui::TextDisabled("Tamanho no Disco: %.2f KB", fileSize / 1024.0f);
                ImGui::Separator();
                ImGui::Spacing();

                if (m_InspectedTexturePath != assetPath.string()) {
                    m_InspectedTexture = Texture2D::Create(assetPath.string());
                    m_InspectedTexturePath = assetPath.string();
                }

                if (m_InspectedTexture && m_InspectedTexture->IsLoaded()) {
                    ImGui::Text("Dimensoes: %u x %u px", m_InspectedTexture->GetWidth(), m_InspectedTexture->GetHeight());
                    ImGui::Spacing();
                    float aspect = (float)m_InspectedTexture->GetHeight() / (float)m_InspectedTexture->GetWidth();
                    float w = 220.0f;
                    float h = w * aspect;
                    ImGui::Image((ImTextureID)(uint64_t)m_InspectedTexture->GetRendererID(), ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.65f, 1.0f));
                if (ImGui::Button("Criar Material a partir desta Textura", ImVec2(-1.0f, 30.0f))) {
                    std::string matName = assetPath.stem().string() + "_Mat";
                    std::string matPath = "assets/materials/" + matName + ".ovmat";
                    auto newMat = Material::Create(matName);
                    newMat->AlbedoTexturePath = assetPath.string();
                    newMat->AlbedoTexture = m_InspectedTexture;
                    newMat->Save(matPath);
                    OVERFLOW_INFO("Material criado: '{0}'!", matPath);
                }
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 6. AUDIO (.wav, .mp3, .ogg)
        // ---------------------------------------------------------------------
        else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.45f, 0.30f, 0.9f));
            if (ImGui::CollapsingHeader("[Audio Clip Asset] Som e Efeito Sonoro", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.50f, 0.95f, 0.55f, 1.0f), "Clip de Audio: %s", filename.c_str());
                ImGui::TextDisabled("Caminho: %s", assetPath.string().c_str());
                ImGui::TextDisabled("Tamanho no Disco: %.2f KB", fileSize / 1024.0f);
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.35f, 1.0f));
                if (ImGui::Button("Tocar Pre-visualizacao (Play)", ImVec2(160.0f, 30.0f))) {
                    AudioEngine::PlayCollisionSound(1.0f);
                }
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Adicionar Emissor de Audio na Cena", ImVec2(-1.0f, 28.0f))) {
                    if (m_Context) {
                        Entity audioEntity = m_Context->CreateEntity(assetPath.stem().string() + "_Audio");
                        auto& as = audioEntity.AddComponent<AudioSourceComponent>();
                        as.ClipPath = assetPath.string();
                        as.ClipName = assetPath.stem().string();
                        SetSelectedEntity(audioEntity);
                    }
                }
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // 7. CENA (.ovscene, .scene)
        // ---------------------------------------------------------------------
        else if (ext == ".ovscene" || ext == ".scene") {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.45f, 0.25f, 0.20f, 0.9f));
            if (ImGui::CollapsingHeader("[Scene Asset] Arquivo de Cena", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.45f, 1.0f), "Cena: %s", filename.c_str());
                ImGui::TextDisabled("Caminho: %s", assetPath.string().c_str());
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.20f, 1.0f));
                if (ImGui::Button("Carregar esta Cena no Editor", ImVec2(-1.0f, 32.0f))) {
                    if (m_Context) {
                        SceneSerializer serializer(m_Context);
                        serializer.Deserialize(assetPath.string());
                        OVERFLOW_INFO("Cena '{0}' carregada!", assetPath.string());
                    }
                }
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();
        }
        // ---------------------------------------------------------------------
        // OUTROS ARQUIVOS / DADOS GERAIS
        // ---------------------------------------------------------------------
        else {
            ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.75f, 1.0f), "[Arquivo Generico] %s", filename.c_str());
            ImGui::TextDisabled("Extensao: %s", ext.c_str());
            ImGui::TextDisabled("Tamanho: %llu bytes", fileSize);
        }
    }

    void SceneHierarchyPanel::OnImGuiRender() {
        // ---------------------------------------------------------------------
        // Painel: World Outliner (Nomenclatura e Identidade Propria Overflow Engine)
        // ---------------------------------------------------------------------
        ImGui::Begin("World Outliner");
        m_HierarchyFocused = ImGui::IsWindowFocused();

        // Barra Superior: Busca de Entidades (Sem botão + Add, criação via Botão Direito ou Menu GameObject)
        static char s_SearchFilter[128] = "";
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(availWidth);
        ImGui::InputTextWithHint("##HierarchySearch", "Search Entities...", s_SearchFilter, sizeof(s_SearchFilter));

        ImGui::Separator();

        if (m_Context) {
            std::string search = s_SearchFilter;
            for (auto& c : search) c = static_cast<char>(tolower(c));

            ImGui::BeginChild("HierarchyTreeRegion", ImVec2(0, 0), false);

            for (auto entity : m_Context->GetEntities()) {
                if (entity.GetComponent<TransformComponent>().ParentID == 0) {
                    if (!search.empty()) {
                        std::string tag = entity.GetComponent<TagComponent>().Tag;
                        for (auto& c : tag) c = static_cast<char>(tolower(c));
                        if (tag.find(search) == std::string::npos)
                            continue;
                    }
                    DrawEntityNode(entity);
                }
            }

            // Deselecionar ao clicar no espaço vazio da janela
            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
                m_SelectionContext = {};
            }

            // Soltar no espaço vazio da hierarquia para mover a entidade de volta para a Raiz
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY")) {
                    uint32_t droppedEntityID = *(const uint32_t*)payload->Data;
                    Entity droppedEntity(droppedEntityID, m_Context.get());
                    droppedEntity.GetComponent<TransformComponent>().ParentID = 0;
                }
                ImGui::EndDragDropTarget();
            }

            // Menu de Contexto (Botão Direito no espaço vazio da hierarquia)
            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Create Empty Entity")) {
                    auto created = m_Context->CreateEntity("Empty Entity");
                    m_SelectionContext = created;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Main Camera")) {
                    auto created = m_Context->CreateCamera("Main Camera", { 0.0f, 2.0f, 8.0f });
                    m_SelectionContext = created;
                }
                if (ImGui::BeginMenu("Camera")) {
                    if (ImGui::MenuItem("Main Camera")) {
                        auto created = m_Context->CreateCamera("Main Camera", { 0.0f, 2.0f, 8.0f });
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Camera")) {
                        auto created = m_Context->CreateCamera("Camera", { 0.0f, 2.0f, 8.0f });
                        m_SelectionContext = created;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("3D Object")) {
                    if (ImGui::MenuItem("Cube")) {
                        auto created = m_Context->CreateCube("Cube");
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Sphere")) {
                        auto created = m_Context->CreateSphere("Sphere");
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Plane")) {
                        auto created = m_Context->CreatePlane("Plane");
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Torus (.obj)")) {
                        auto created = m_Context->CreateEntity("Torus");
                        auto& mc = created.AddComponent<MeshComponent>(MeshGeometryType::Torus);
                        mc.ModelAsset = m_Context->GetSharedTorus();
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Procedural Terrain")) {
                        auto created = m_Context->CreateTerrain("Procedural Terrain", 40, 24.0f, 2.5f);
                        m_SelectionContext = created;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Light")) {
                    if (ImGui::MenuItem("Directional Light")) {
                        auto created = m_Context->CreateDirectionalLight("Directional Light");
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Point Light")) {
                        auto created = m_Context->CreateEntity("Point Light");
                        created.GetComponent<TransformComponent>().Translation = { 0.0f, 2.0f, 0.0f };
                        auto& lc = created.AddComponent<LightComponent>();
                        lc.Type = LightType::Point;
                        lc.Color = { 1.0f, 0.85f, 0.6f };
                        lc.Intensity = 3.0f;
                        lc.Range = 12.0f;
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Spot Light")) {
                        auto created = m_Context->CreateEntity("Spot Light");
                        created.GetComponent<TransformComponent>().Translation = { 0.0f, 4.0f, 0.0f };
                        created.GetComponent<TransformComponent>().Rotation = { 45.0f, 0.0f, 0.0f };
                        auto& lc = created.AddComponent<LightComponent>();
                        lc.Type = LightType::Spot;
                        lc.Color = { 0.8f, 0.9f, 1.0f };
                        lc.Intensity = 5.0f;
                        lc.Range = 20.0f;
                        lc.InnerAngle = 18.0f;
                        lc.OuterAngle = 32.0f;
                        m_SelectionContext = created;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Effects")) {
                    if (ImGui::MenuItem("Particle System")) {
                        auto created = m_Context->CreateEntity("Particle System");
                        created.GetComponent<TransformComponent>().Translation = { 0.0f, 1.0f, 0.0f };
                        created.AddComponent<ParticleSystemComponent>();
                        m_SelectionContext = created;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Audio")) {
                    if (ImGui::MenuItem("Audio Source")) {
                        auto created = m_Context->CreateEntity("Audio Source");
                        created.AddComponent<AudioSourceComponent>();
                        m_SelectionContext = created;
                    }
                    if (ImGui::MenuItem("Audio Listener")) {
                        auto created = m_Context->CreateEntity("Audio Listener");
                        created.AddComponent<AudioListenerComponent>();
                        m_SelectionContext = created;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            ImGui::EndChild();
        }

        ImGui::End();

        // ---------------------------------------------------------------------
        // Painel: Inspector (Propriedades, Componentes & Assets)
        // ---------------------------------------------------------------------
        ImGui::Begin("Inspector");

        if (m_SelectionContext) {
            DrawComponents(m_SelectionContext);

            // Drag & Drop direto no Inspector (Anexa Scripts, Materiais, Texturas)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                    std::string pathStr = (const char*)payload->Data;
                    std::filesystem::path path(pathStr);
                    std::string ext = path.extension().string();
                    for (auto& c : ext) c = static_cast<char>(tolower(c));

                    if (ext == ".h" || ext == ".cpp") {
                        std::string scriptName = path.stem().string();
                        if (!m_SelectionContext.HasComponent<NativeScriptComponent>()) {
                            m_SelectionContext.AddComponent<NativeScriptComponent>();
                        }
                        m_SelectionContext.GetComponent<NativeScriptComponent>().BindByName(scriptName);
                        OVERFLOW_INFO("Script '{0}' anexado a entidade '{1}' via Inspector!", scriptName, m_SelectionContext.GetComponent<TagComponent>().Tag);
                    } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                        if (m_SelectionContext.HasComponent<MeshComponent>()) {
                            m_SelectionContext.GetComponent<MeshComponent>().TextureAsset = Texture2D::Create(path.string());
                            OVERFLOW_INFO("Textura aplicada a entidade '{0}'!", m_SelectionContext.GetComponent<TagComponent>().Tag);
                        }
                    } else if (ext == ".ovmat" || ext == ".mat") {
                        if (m_SelectionContext.HasComponent<MeshComponent>()) {
                            m_SelectionContext.GetComponent<MeshComponent>().MaterialAsset = Material::Load(path.string());
                            OVERFLOW_INFO("Material '{0}' aplicado a entidade '{1}'!", path.string(), m_SelectionContext.GetComponent<TagComponent>().Tag);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        } else if (!m_SelectedAsset.empty()) {
            DrawAssetInspector(m_SelectedAsset);
        } else {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.68f, 1.0f));
            ImGui::Text("Nenhum objeto ou asset selecionado.");
            ImGui::Spacing();
            ImGui::TextWrapped("Selecione um GameObject no World Outliner ou clique em um asset no Content Browser (Material, Script, Modelo 3D, Textura, Audio) para visualizar e editar suas configuracoes no Inspector.");
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
        auto& tagComp = entity.GetComponent<TagComponent>();
        auto& tag = tagComp.Tag;

        std::vector<Entity> children;
        for (auto other : m_Context->GetEntities()) {
            if (other != entity && other.GetComponent<TransformComponent>().ParentID == entity.GetID()) {
                children.push_back(other);
            }
        }

        ImGui::PushID(static_cast<int>(entity.GetID()));

        // 1. Toggle de Visibilidade (Ícone de Olho estilo Unity / Unreal)
        bool isActive = tagComp.IsActive;
        const char* eyeIcon = isActive ? "[v]" : "[-]";
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.90f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.45f, 0.7f));
        }

        if (ImGui::SmallButton(eyeIcon)) {
            tagComp.IsActive = !tagComp.IsActive;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // 2. Nó da Árvore com Ícone de Tipo
        ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0)
                                 | ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        std::string icon = "[Obj]";
        if (entity.HasComponent<LightComponent>()) {
            icon = (entity.GetComponent<LightComponent>().Type == LightType::Directional) ? "[Sun]" : "[Light]";
        } else if (entity.HasComponent<CameraComponent>()) {
            icon = "[Cam]";
        } else if (entity.HasComponent<MeshComponent>()) {
            auto geom = entity.GetComponent<MeshComponent>().Geometry;
            if (geom == MeshGeometryType::Cube) icon = "[Cube]";
            else if (geom == MeshGeometryType::Sphere) icon = "[Sphere]";
            else if (geom == MeshGeometryType::Plane) icon = "[Plane]";
            else if (geom == MeshGeometryType::Torus) icon = "[Torus]";
            else icon = "[Mesh]";
        } else if (!children.empty()) {
            icon = "[Folder]";
        }

        std::string nodeLabel = icon + " " + tag;

        if (!isActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.55f, 0.7f));
        }

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", nodeLabel.c_str());

        if (!isActive) {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked()) {
            m_SelectionContext = entity;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_SelectionContext = entity;
            m_EntityDoubleClicked = true;
        }

        // 3. Drag & Drop
        if (ImGui::BeginDragDropSource()) {
            uint32_t entityID = entity.GetID();
            ImGui::SetDragDropPayload("SCENE_HIERARCHY_ENTITY", &entityID, sizeof(uint32_t));
            ImGui::Text("Move %s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY")) {
                uint32_t droppedEntityID = *(const uint32_t*)payload->Data;
                if (droppedEntityID != entity.GetID()) {
                    Entity droppedEntity(droppedEntityID, m_Context.get());
                    droppedEntity.GetComponent<TransformComponent>().ParentID = entity.GetID();
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::string pathStr = (const char*)payload->Data;
                std::filesystem::path path(pathStr);
                std::string ext = path.extension().string();
                for (auto& c : ext) c = static_cast<char>(tolower(c));

                if (ext == ".h" || ext == ".cpp") {
                    std::string scriptName = path.stem().string();
                    if (!entity.HasComponent<NativeScriptComponent>()) {
                        entity.AddComponent<NativeScriptComponent>();
                    }
                    entity.GetComponent<NativeScriptComponent>().BindByName(scriptName);
                    m_SelectionContext = entity;
                    OVERFLOW_INFO("Script '{0}' anexado a entidade '{1}' via World Outliner!", scriptName, entity.GetComponent<TagComponent>().Tag);
                } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                    if (entity.HasComponent<MeshComponent>()) {
                        entity.GetComponent<MeshComponent>().TextureAsset = Texture2D::Create(path.string());
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        bool entityDeleted = false;
        bool duplicateEntity = false;

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Duplicate Entity")) {
                duplicateEntity = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Entity")) {
                entityDeleted = true;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            for (auto child : children) {
                DrawEntityNode(child);
            }
            ImGui::TreePop();
        }

        if (duplicateEntity) {
            auto newEntity = m_Context->CreateEntity(tag + " (Copy)");
            if (entity.HasComponent<TransformComponent>()) {
                auto& tc = newEntity.GetComponent<TransformComponent>();
                tc = entity.GetComponent<TransformComponent>();
                tc.Translation += glm::vec3(0.5f, 0.0f, 0.5f); // Pequeno offset
            }
            if (entity.HasComponent<MeshComponent>()) {
                newEntity.AddComponent<MeshComponent>() = entity.GetComponent<MeshComponent>();
            }
            if (entity.HasComponent<LightComponent>()) {
                newEntity.AddComponent<LightComponent>() = entity.GetComponent<LightComponent>();
            }
            m_SelectionContext = newEntity;
        }

        if (entityDeleted) {
            m_Context->DestroyEntity(entity);
            if (m_SelectionContext == entity) {
                m_SelectionContext = {};
            }
        }

        ImGui::PopID();
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity) {
        // ---------------------------------------------------------------------
        // Cabeçalho da Entidade estilo Unity / Unreal
        // ---------------------------------------------------------------------
        if (entity.HasComponent<TagComponent>()) {
            auto& tagComp = entity.GetComponent<TagComponent>();

            ImGui::Checkbox("##EntityIsActive", &tagComp.IsActive);
            ImGui::SameLine();

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strncpy_s(buffer, sizeof(buffer), tagComp.Tag.c_str(), sizeof(buffer) - 1);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
            if (ImGui::InputText("##EntityName", buffer, sizeof(buffer))) {
                tagComp.Tag = std::string(buffer);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("ID: %u", entity.GetID());

            // Sub-linha de Tag e Layer
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 110.0f);
            ImGui::TextDisabled("Tag:");
            ImGui::SameLine();
            const char* tags[] = { "Untagged", "MainCamera", "Player", "Prop", "Light", "Environment" };
            static int currentTag = 0;
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##TagCombo", &currentTag, tags, IM_ARRAYSIZE(tags));

            ImGui::NextColumn();
            ImGui::TextDisabled("Layer:");
            ImGui::SameLine();
            const char* layers[] = { "Default", "TransparentFX", "Ignore Raycast", "Water", "UI" };
            static int currentLayer = 0;
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##LayerCombo", &currentLayer, layers, IM_ARRAYSIZE(layers));
            ImGui::Columns(1);

            ImGui::PopStyleVar();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // ---------------------------------------------------------------------
        // Transform Component
        // ---------------------------------------------------------------------
        if (entity.HasComponent<TransformComponent>()) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& transform = entity.GetComponent<TransformComponent>();

                DrawVec3Control("Position", transform.Translation, 0.0f);
                DrawVec3Control("Rotation", transform.Rotation, 0.0f);
                DrawVec3Control("Scale", transform.Scale, 1.0f);

                // Parent-Child Relationship
                ImGui::Spacing();
                std::string parentName = "None (Root)";
                if (transform.ParentID != 0 && m_Context->HasComponent<TagComponent>(transform.ParentID)) {
                    parentName = m_Context->GetComponent<TagComponent>(transform.ParentID).Tag;
                }

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Parent");
                ImGui::NextColumn();

                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##ParentCombo", parentName.c_str())) {
                    if (ImGui::Selectable("None (Root)", transform.ParentID == 0)) {
                        transform.ParentID = 0;
                    }

                    for (auto other : m_Context->GetEntities()) {
                        if (other != entity) {
                            auto& otherTag = other.GetComponent<TagComponent>().Tag;
                            bool isSelected = (transform.ParentID == other.GetID());
                            if (ImGui::Selectable(otherTag.c_str(), isSelected)) {
                                transform.ParentID = other.GetID();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Columns(1);
            }
            ImGui::PopStyleColor();
        }

        // ---------------------------------------------------------------------
        // Mesh Component (Mesh Renderer estilo Unity)
        // ---------------------------------------------------------------------
        if (entity.HasComponent<MeshComponent>()) {
            bool removeMesh = false;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.24f, 0.30f, 1.0f));
            if (ImGui::CollapsingHeader("Mesh Renderer (PBR)", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& mesh = entity.GetComponent<MeshComponent>();

                const char* geomTypes[] = { "Cube", "Sphere", "Plane", "Torus (.obj)", "Custom File..." };
                int currentGeom = static_cast<int>(mesh.Geometry);
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Mesh");
                ImGui::NextColumn();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##Geometry", &currentGeom, geomTypes, IM_ARRAYSIZE(geomTypes))) {
                    mesh.Geometry = static_cast<MeshGeometryType>(currentGeom);
                    if (mesh.Geometry == MeshGeometryType::Cube) mesh.ModelAsset = m_Context->GetSharedCube();
                    else if (mesh.Geometry == MeshGeometryType::Sphere) mesh.ModelAsset = m_Context->GetSharedSphere();
                    else if (mesh.Geometry == MeshGeometryType::Plane) mesh.ModelAsset = m_Context->GetSharedPlane();
                    else if (mesh.Geometry == MeshGeometryType::Torus) mesh.ModelAsset = m_Context->GetSharedTorus();
                }
                ImGui::Columns(1);

                if (mesh.Geometry == MeshGeometryType::CustomOBJ) {
                    char pathBuffer[256];
                    memset(pathBuffer, 0, sizeof(pathBuffer));
                    strncpy_s(pathBuffer, sizeof(pathBuffer), mesh.CustomFilePath.c_str(), sizeof(pathBuffer) - 1);

                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 85.0f);
                    ImGui::Text("File");
                    ImGui::NextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
                    if (ImGui::InputText("##OBJPath", pathBuffer, sizeof(pathBuffer))) {
                        mesh.CustomFilePath = std::string(pathBuffer);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load", ImVec2(60.0f, 0.0f))) {
                        mesh.ModelAsset = Model::Create(mesh.CustomFilePath);
                    }
                    ImGui::Columns(1);
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.70f, 1.0f), "Material Asset (.ovmat)");

                std::string matLabel = mesh.MaterialAsset ? mesh.MaterialAsset->Name : "Default Material (Embedded)";
                if (!mesh.MaterialPath.empty()) {
                    matLabel += " (" + std::filesystem::path(mesh.MaterialPath).filename().string() + ")";
                }

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Material");
                ImGui::NextColumn();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.20f, 0.18f, 1.0f));
                ImGui::Button(matLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 24.0f));
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        if (pathStr.ends_with(".ovmat")) {
                            mesh.MaterialPath = pathStr;
                            mesh.MaterialAsset = Material::Load(pathStr);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("New", ImVec2(40.0f, 24.0f))) {
                    std::string newPath = "assets/materials/" + entity.GetComponent<TagComponent>().Tag + "_Mat.ovmat";
                    auto newMat = Material::Create(entity.GetComponent<TagComponent>().Tag + "_Mat");
                    newMat->AlbedoColor = mesh.Color;
                    newMat->Metallic = mesh.Metallic;
                    newMat->Roughness = mesh.Roughness;
                    newMat->Save(newPath);
                    mesh.MaterialPath = newPath;
                    mesh.MaterialAsset = newMat;
                }
                ImGui::SameLine();
                if (ImGui::Button("X", ImVec2(24.0f, 24.0f))) {
                    mesh.MaterialAsset = nullptr;
                    mesh.MaterialPath = "";
                }
                ImGui::Columns(1);

                // Seletor de Shader (Standard PBR, Unlit, Blinn-Phong, Wireframe)
                const char* shaderNames[] = { "Standard PBR", "Unlit", "Blinn-Phong", "Wireframe" };
                int currentShader = mesh.MaterialAsset ? static_cast<int>(mesh.MaterialAsset->Type) : 0;
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Shader");
                ImGui::NextColumn();
                if (ImGui::Combo("##MaterialShader", &currentShader, shaderNames, IM_ARRAYSIZE(shaderNames))) {
                    if (mesh.MaterialAsset) {
                        mesh.MaterialAsset->Type = static_cast<ShaderType>(currentShader);
                        mesh.MaterialAsset->Save();
                    }
                }
                ImGui::Columns(1);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "Surface Parameters");

                // Base Color / Albedo
                glm::vec4* colorPtr = mesh.MaterialAsset ? &mesh.MaterialAsset->AlbedoColor : &mesh.Color;
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Albedo");
                ImGui::NextColumn();
                if (ImGui::ColorEdit4("##BaseColor", glm::value_ptr(*colorPtr), ImGuiColorEditFlags_NoInputs)) {
                    if (mesh.MaterialAsset) {
                        mesh.Color = mesh.MaterialAsset->AlbedoColor;
                        mesh.MaterialAsset->Save();
                    }
                }
                ImGui::Columns(1);

                // Texture Slot estilo Unity
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Texture");
                ImGui::NextColumn();

                auto currentTex = mesh.MaterialAsset ? mesh.MaterialAsset->AlbedoTexture : mesh.TextureAsset;
                std::string texLabel = currentTex ? currentTex->GetPath() : "None (Drag Texture Here)";
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
                ImGui::Button(texLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 24.0f));
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        auto newTex = Texture2D::Create(pathStr);
                        if (mesh.MaterialAsset) {
                            mesh.MaterialAsset->AlbedoTexture = newTex;
                            mesh.MaterialAsset->AlbedoTexturePath = pathStr;
                            mesh.MaterialAsset->Save();
                        } else {
                            mesh.TextureAsset = newTex;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("Demo", ImVec2(40.0f, 24.0f))) {
                    auto demoTex = Texture2D::Create("assets/textures/uv_checker.bmp");
                    if (mesh.MaterialAsset) {
                        mesh.MaterialAsset->AlbedoTexture = demoTex;
                        mesh.MaterialAsset->AlbedoTexturePath = "assets/textures/uv_checker.bmp";
                        mesh.MaterialAsset->Save();
                    } else {
                        mesh.TextureAsset = demoTex;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("X##Tex", ImVec2(24.0f, 24.0f))) {
                    if (mesh.MaterialAsset) {
                        mesh.MaterialAsset->AlbedoTexture = nullptr;
                        mesh.MaterialAsset->AlbedoTexturePath = "";
                        mesh.MaterialAsset->Save();
                    } else {
                        mesh.TextureAsset = nullptr;
                    }
                }
                ImGui::Columns(1);

                // PBR Sliders
                float* metalPtr = mesh.MaterialAsset ? &mesh.MaterialAsset->Metallic : &mesh.Metallic;
                float* roughPtr = mesh.MaterialAsset ? &mesh.MaterialAsset->Roughness : &mesh.Roughness;
                float* aoPtr = mesh.MaterialAsset ? &mesh.MaterialAsset->AO : &mesh.AO;
                float* tilePtr = mesh.MaterialAsset ? &mesh.MaterialAsset->Tiling : &mesh.Tiling;

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Metallic");
                ImGui::NextColumn();
                if (ImGui::SliderFloat("##Metallic", metalPtr, 0.0f, 1.0f, "%.2f")) {
                    if (mesh.MaterialAsset) mesh.MaterialAsset->Save();
                }
                ImGui::NextColumn();

                ImGui::Text("Roughness");
                ImGui::NextColumn();
                if (ImGui::SliderFloat("##Roughness", roughPtr, 0.01f, 1.0f, "%.2f")) {
                    if (mesh.MaterialAsset) mesh.MaterialAsset->Save();
                }
                ImGui::NextColumn();

                ImGui::Text("AO");
                ImGui::NextColumn();
                if (ImGui::SliderFloat("##AO", aoPtr, 0.0f, 1.0f, "%.2f")) {
                    if (mesh.MaterialAsset) mesh.MaterialAsset->Save();
                }
                ImGui::NextColumn();

                ImGui::Text("Tiling");
                ImGui::NextColumn();
                if (ImGui::DragFloat("##Tiling", tilePtr, 0.1f, 0.1f, 20.0f, "%.1f")) {
                    if (mesh.MaterialAsset) mesh.MaterialAsset->Save();
                }
                ImGui::Columns(1);

                // Slot de Normal Map (Relevo PBR 3D)
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Normal Map");
                ImGui::NextColumn();

                auto currentNorm = mesh.MaterialAsset ? mesh.MaterialAsset->NormalMap : mesh.NormalMapAsset;
                std::string normLabel = currentNorm ? currentNorm->GetPath() : "None (Drag Normal Map)";
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
                ImGui::Button(normLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 24.0f));
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        auto newNorm = Texture2D::Create(pathStr);
                        if (mesh.MaterialAsset) {
                            mesh.MaterialAsset->NormalMap = newNorm;
                            mesh.MaterialAsset->NormalMapPath = pathStr;
                            mesh.MaterialAsset->UseNormalMap = true;
                            mesh.MaterialAsset->Save();
                        } else {
                            mesh.NormalMapAsset = newNorm;
                            mesh.UseNormalMap = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("X##Norm", ImVec2(24.0f, 24.0f))) {
                    if (mesh.MaterialAsset) {
                        mesh.MaterialAsset->NormalMap = nullptr;
                        mesh.MaterialAsset->NormalMapPath = "";
                        mesh.MaterialAsset->UseNormalMap = false;
                        mesh.MaterialAsset->Save();
                    } else {
                        mesh.NormalMapAsset = nullptr;
                        mesh.UseNormalMap = false;
                    }
                }
                ImGui::Columns(1);

                bool* useNormPtr = mesh.MaterialAsset ? &mesh.MaterialAsset->UseNormalMap : &mesh.UseNormalMap;
                if (ImGui::Checkbox("Enable Normal Mapping", useNormPtr)) {
                    if (mesh.MaterialAsset) mesh.MaterialAsset->Save();
                }

                // Efeito Emissivo (Neon / Glow Bloom)
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Emission");
                ImGui::NextColumn();
                ImGui::ColorEdit3("##EmissionColor", glm::value_ptr(mesh.EmissionColor));
                ImGui::NextColumn();

                ImGui::Text("Glow Boost");
                ImGui::NextColumn();
                ImGui::SliderFloat("##EmissionIntensity", &mesh.EmissionIntensity, 0.0f, 10.0f, "%.2f (Glow)");
                ImGui::Columns(1);

                ImGui::Spacing();
                if (ImGui::Button("Remove Mesh Component", ImVec2(-1, 0))) {
                    removeMesh = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeMesh) {
                entity.RemoveComponent<MeshComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Light Component
        // ---------------------------------------------------------------------
        if (entity.HasComponent<LightComponent>()) {
            bool removeLight = false;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.22f, 0.16f, 1.0f));
            if (ImGui::CollapsingHeader("Light (PBR)", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& light = entity.GetComponent<LightComponent>();

                const char* lightTypes[] = { "Directional", "Point", "Spot" };
                int currentType = static_cast<int>(light.Type);
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 95.0f);
                ImGui::Text("Type");
                ImGui::NextColumn();
                if (ImGui::Combo("##LightType", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
                    light.Type = static_cast<LightType>(currentType);
                }
                ImGui::NextColumn();

                ImGui::Text("Color");
                ImGui::NextColumn();
                ImGui::ColorEdit3("##LightColor", glm::value_ptr(light.Color));
                ImGui::NextColumn();

                ImGui::Text("Intensity");
                ImGui::NextColumn();
                ImGui::DragFloat("##Intensity", &light.Intensity, 0.05f, 0.0f, 30.0f, "%.2f");
                ImGui::NextColumn();

                if (light.Type == LightType::Point || light.Type == LightType::Spot) {
                    ImGui::Text("Range");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##LightRange", &light.Range, 0.2f, 0.5f, 100.0f, "%.1f m");
                    ImGui::NextColumn();
                }

                if (light.Type == LightType::Spot) {
                    ImGui::Text("Inner Cone");
                    ImGui::NextColumn();
                    ImGui::SliderFloat("##InnerAngle", &light.InnerAngle, 1.0f, 89.0f, "%.1f deg");
                    ImGui::NextColumn();

                    ImGui::Text("Outer Cone");
                    ImGui::NextColumn();
                    ImGui::SliderFloat("##OuterAngle", &light.OuterAngle, light.InnerAngle + 1.0f, 90.0f, "%.1f deg");
                    ImGui::NextColumn();
                }

                if (light.Type == LightType::Directional) {
                    ImGui::Text("Ambient");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##Ambient", &light.AmbientIntensity, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "Real-time Shadows");
                ImGui::Checkbox("Cast Shadows", &light.CastShadows);
                if (light.CastShadows) {
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 85.0f);
                    ImGui::Text("Bias");
                    ImGui::NextColumn();
                    ImGui::SliderFloat("##ShadowBias", &light.Bias, 0.0005f, 0.02f, "%.4f");
                    ImGui::Columns(1);
                }

                if (light.Type == LightType::Directional && m_Context) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.0f, 1.0f), "Nuvens Volumetricas 3D & Atmosfera");

                    bool enableClouds = m_Context->GetVolumetricCloudsEnabled();
                    if (ImGui::Checkbox("Habilitar Nuvens Volumetricas 3D", &enableClouds)) {
                        m_Context->SetVolumetricCloudsEnabled(enableClouds);
                    }

                    if (enableClouds) {
                        float dens = m_Context->GetCloudDensityMultiplier();
                        if (ImGui::SliderFloat("Densidade das Nuvens", &dens, 0.0f, 4.0f, "%.2fx")) {
                            m_Context->SetCloudDensityMultiplier(dens);
                        }

                        float cov = m_Context->GetCloudCoverage();
                        if (ImGui::SliderFloat("Cobertura do Ceu (Coverage)", &cov, 0.0f, 1.0f, "%.2f")) {
                            m_Context->SetCloudCoverage(cov);
                        }

                        float baseAlt = m_Context->GetCloudBaseAltitude();
                        float topAlt = m_Context->GetCloudTopAltitude();
                        bool altChanged = false;
                        if (ImGui::DragFloat("Altitude Base", &baseAlt, 1.0f, 10.0f, 300.0f, "%.0f m")) altChanged = true;
                        if (ImGui::DragFloat("Altitude Topo", &topAlt, 1.0f, baseAlt + 10.0f, 800.0f, "%.0f m")) altChanged = true;
                        if (altChanged) {
                            m_Context->SetCloudAltitudes(baseAlt, topAlt);
                        }

                        float wind = m_Context->GetWindSpeed();
                        if (ImGui::SliderFloat("Velocidade do Vento", &wind, 0.0f, 20.0f, "%.1f m/s")) {
                            m_Context->SetWindSpeed(wind);
                        }
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Remove Light Component", ImVec2(-1, 0))) {
                    removeLight = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeLight) {
                entity.RemoveComponent<LightComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Camera Component (Estilo Unity / Unreal)
        // ---------------------------------------------------------------------
        if (entity.HasComponent<CameraComponent>()) {
            ImGui::Spacing();
            bool removeCamera = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.22f, 0.28f, 0.9f));
            if (ImGui::CollapsingHeader("[Camera] Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& camComp = entity.GetComponent<CameraComponent>();

                ImGui::Checkbox("Primary Camera", &camComp.Primary);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Define esta camera como a visualizacao principal na Janela Game");
                ImGui::SameLine();
                ImGui::Checkbox("Show Frustum", &camComp.DrawFrustum);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exibir a piramide de visao da camera na Viewport");

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 95.0f);

                ImGui::Text("Field of View");
                ImGui::NextColumn();
                float fov = camComp.Camera.GetFOV();
                if (ImGui::SliderFloat("##FOV", &fov, 25.0f, 120.0f, "%.1f deg")) {
                    camComp.Camera = PerspectiveCamera(fov, camComp.Camera.GetAspectRatio(), camComp.Camera.GetNearClip(), camComp.Camera.GetFarClip());
                }
                ImGui::NextColumn();

                ImGui::Text("Near Clip");
                ImGui::NextColumn();
                float nearClip = camComp.Camera.GetNearClip();
                if (ImGui::DragFloat("##NearClip", &nearClip, 0.05f, 0.01f, 10.0f, "%.2f")) {
                    camComp.Camera = PerspectiveCamera(camComp.Camera.GetFOV(), camComp.Camera.GetAspectRatio(), nearClip, camComp.Camera.GetFarClip());
                }
                ImGui::NextColumn();

                ImGui::Text("Far Clip");
                ImGui::NextColumn();
                float farClip = camComp.Camera.GetFarClip();
                if (ImGui::DragFloat("##FarClip", &farClip, 10.0f, 10.0f, 10000.0f, "%.1f")) {
                    camComp.Camera = PerspectiveCamera(camComp.Camera.GetFOV(), camComp.Camera.GetAspectRatio(), camComp.Camera.GetNearClip(), farClip);
                }
                ImGui::Columns(1);

                ImGui::Spacing();
                if (ImGui::Button("Remove Camera Component", ImVec2(-1, 0))) {
                    removeCamera = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeCamera) {
                entity.RemoveComponent<CameraComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Rigidbody 3D Component (Física Dinâmica estilo Unity / Unreal)
        // ---------------------------------------------------------------------
        if (entity.HasComponent<RigidbodyComponent>()) {
            ImGui::Spacing();
            bool removeRB = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.20f, 0.16f, 0.9f));
            if (ImGui::CollapsingHeader("[Physics] Rigidbody 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& rb = entity.GetComponent<RigidbodyComponent>();

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 105.0f);

                ImGui::Text("Body Type");
                ImGui::NextColumn();
                const char* bodyTypes[] = { "Dynamic", "Static", "Kinematic" };
                int currentType = static_cast<int>(rb.Type);
                if (ImGui::Combo("##BodyType", &currentType, bodyTypes, IM_ARRAYSIZE(bodyTypes))) {
                    rb.Type = static_cast<BodyType>(currentType);
                }
                ImGui::NextColumn();

                if (rb.Type == BodyType::Dynamic) {
                    ImGui::Text("Mass (kg)");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##Mass", &rb.Mass, 0.1f, 0.01f, 1000.0f, "%.2f kg");
                    ImGui::NextColumn();

                    ImGui::Text("Linear Drag");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##LinearDrag", &rb.LinearDrag, 0.01f, 0.0f, 10.0f, "%.2f");
                    ImGui::NextColumn();

                    ImGui::Text("Angular Drag");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##AngularDrag", &rb.AngularDrag, 0.01f, 0.0f, 10.0f, "%.2f");
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);

                if (rb.Type == BodyType::Dynamic) {
                    ImGui::Checkbox("Use Gravity", &rb.UseGravity);
                    ImGui::SameLine();
                    ImGui::Checkbox("Is Kinematic", &rb.IsKinematic);

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.70f, 0.35f, 1.0f), "Constraints (Travar Eixos)");
                    ImGui::Text("Freeze Position:");
                    ImGui::SameLine();
                    ImGui::Checkbox("X##FPX", &rb.FreezePositionX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y##FPY", &rb.FreezePositionY);
                    ImGui::SameLine();
                    ImGui::Checkbox("Z##FPZ", &rb.FreezePositionZ);

                    ImGui::Text("Freeze Rotation:");
                    ImGui::SameLine();
                    ImGui::Checkbox("X##FRX", &rb.FreezeRotationX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y##FRY", &rb.FreezeRotationY);
                    ImGui::SameLine();
                    ImGui::Checkbox("Z##FRZ", &rb.FreezeRotationZ);
                }

                ImGui::Spacing();
                if (ImGui::Button("Remove Rigidbody", ImVec2(-1, 0))) {
                    removeRB = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeRB) {
                entity.RemoveComponent<RigidbodyComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Collider Component (Box / Sphere Collider)
        // ---------------------------------------------------------------------
        if (entity.HasComponent<ColliderComponent>()) {
            ImGui::Spacing();
            bool removeCol = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.25f, 0.20f, 0.9f));
            if (ImGui::CollapsingHeader("[Physics] Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& col = entity.GetComponent<ColliderComponent>();

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 105.0f);

                ImGui::Text("Type");
                ImGui::NextColumn();
                const char* colTypes[] = { "Box Collider", "Sphere Collider" };
                int currentColType = static_cast<int>(col.Type);
                if (ImGui::Combo("##ColType", &currentColType, colTypes, IM_ARRAYSIZE(colTypes))) {
                    col.Type = static_cast<ColliderType>(currentColType);
                }
                ImGui::NextColumn();

                if (col.Type == ColliderType::Sphere) {
                    ImGui::Text("Radius");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##ColRadius", &col.Radius, 0.05f, 0.05f, 50.0f, "%.2f");
                    ImGui::NextColumn();
                }

                ImGui::Text("Bounciness");
                ImGui::NextColumn();
                ImGui::SliderFloat("##Bounciness", &col.Bounciness, 0.0f, 1.0f, "%.2f (Restitution)");
                ImGui::NextColumn();

                ImGui::Text("Friction");
                ImGui::NextColumn();
                ImGui::SliderFloat("##Friction", &col.Friction, 0.0f, 1.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Columns(1);

                DrawVec3Control("Center", col.Center, 0.0f, 105.0f);

                if (col.Type == ColliderType::Box) {
                    DrawVec3Control("Size", col.Size, 1.0f, 105.0f);
                }

                ImGui::Checkbox("Is Trigger", &col.IsTrigger);

                ImGui::Spacing();
                if (ImGui::Button("Remove Collider", ImVec2(-1, 0))) {
                    removeCol = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeCol) {
                entity.RemoveComponent<ColliderComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Particle System Component
        // ---------------------------------------------------------------------
        if (entity.HasComponent<ParticleSystemComponent>()) {
            ImGui::Spacing();
            bool removePS = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.15f, 0.25f, 0.9f));
            if (ImGui::CollapsingHeader("[Effects] Particle System", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& ps = entity.GetComponent<ParticleSystemComponent>();

                ImGui::Checkbox("Playing", &ps.IsPlaying);
                ImGui::SameLine();
                ImGui::TextDisabled("(Active Particles: %zu)", ps.Pool.size());

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 110.0f);

                ImGui::Text("Max Particles");
                ImGui::NextColumn();
                ImGui::DragInt("##MaxParticles", &ps.MaxParticles, 5, 10, 2000);
                ImGui::NextColumn();

                ImGui::Text("Emission Rate");
                ImGui::NextColumn();
                ImGui::DragFloat("##EmissionRate", &ps.EmissionRate, 0.5f, 1.0f, 200.0f, "%.1f /s");
                ImGui::NextColumn();

                ImGui::Text("Lifetime");
                ImGui::NextColumn();
                ImGui::DragFloat("##Lifetime", &ps.Lifetime, 0.05f, 0.1f, 10.0f, "%.2f s");
                ImGui::NextColumn();

                ImGui::Text("Start Size");
                ImGui::NextColumn();
                ImGui::DragFloat("##StartSize", &ps.StartSize, 0.01f, 0.01f, 5.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("End Size");
                ImGui::NextColumn();
                ImGui::DragFloat("##EndSize", &ps.EndSize, 0.01f, 0.0f, 5.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("Start Color");
                ImGui::NextColumn();
                ImGui::ColorEdit4("##StartColor", glm::value_ptr(ps.StartColor));
                ImGui::NextColumn();

                ImGui::Text("End Color");
                ImGui::NextColumn();
                ImGui::ColorEdit4("##EndColor", glm::value_ptr(ps.EndColor));
                ImGui::NextColumn();

                ImGui::Columns(1);

                DrawVec3Control("Velocity", ps.BaseVelocity, 0.0f, 110.0f);
                DrawVec3Control("Gravity", ps.Gravity, 0.0f, 110.0f);

                ImGui::Spacing();
                if (ImGui::Button("Remove Particle System", ImVec2(-1, 0))) {
                    removePS = true;
                }
            }
            ImGui::PopStyleColor();

            if (removePS) {
                entity.RemoveComponent<ParticleSystemComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Audio Source Component (Áudio 3D Espacial)
        // ---------------------------------------------------------------------
        if (entity.HasComponent<AudioSourceComponent>()) {
            ImGui::Spacing();
            bool removeAudio = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.28f, 0.32f, 0.9f));
            if (ImGui::CollapsingHeader("[Audio] Audio Source", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& as = entity.GetComponent<AudioSourceComponent>();

                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 105.0f);

                ImGui::Text("Volume");
                ImGui::NextColumn();
                ImGui::SliderFloat("##AudioVolume", &as.Volume, 0.0f, 1.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("Pitch");
                ImGui::NextColumn();
                ImGui::SliderFloat("##AudioPitch", &as.Pitch, 0.2f, 3.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Columns(1);

                ImGui::Checkbox("Loop", &as.Loop);
                ImGui::SameLine();
                ImGui::Checkbox("Play On Awake", &as.PlayOnAwake);
                ImGui::SameLine();
                ImGui::Checkbox("Spatial 3D", &as.Spatial3D);

                ImGui::Spacing();
                if (ImGui::Button("Play Test Sound", ImVec2(-1, 24.0f))) {
                    glm::vec3 pos = entity.GetComponent<TransformComponent>().Translation;
                    AudioEngine::PlayCollisionSound(as.Volume, pos, glm::vec3(0.0f));
                }

                ImGui::Spacing();
                if (ImGui::Button("Remove Audio Source", ImVec2(-1, 0))) {
                    removeAudio = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeAudio) {
                entity.RemoveComponent<AudioSourceComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Audio Listener Component
        // ---------------------------------------------------------------------
        if (entity.HasComponent<AudioListenerComponent>()) {
            ImGui::Spacing();
            bool removeAL = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.22f, 0.28f, 0.9f));
            if (ImGui::CollapsingHeader("[Audio] Audio Listener", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& al = entity.GetComponent<AudioListenerComponent>();
                ImGui::Checkbox("Listener Active", &al.Active);

                ImGui::Spacing();
                if (ImGui::Button("Remove Audio Listener", ImVec2(-1, 0))) {
                    removeAL = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeAL) {
                entity.RemoveComponent<AudioListenerComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Card: Native Script / Behavior Component
        // ---------------------------------------------------------------------
        if (entity.HasComponent<NativeScriptComponent>()) {
            ImGui::Spacing();
            bool removeScript = false;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.28f, 0.50f, 0.9f));
            if (ImGui::CollapsingHeader("[Behavior] C++ EntityBehavior", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& nsc = entity.GetComponent<NativeScriptComponent>();

                // 1. Slot Visual de Drag & Drop para Scripts
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.22f, 0.36f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.35f, 0.55f, 0.9f));
                std::string dropSlotLabel = (nsc.ScriptName.empty() || nsc.ScriptName == "None") 
                    ? "[ Drag & Drop C++ Script (.h) Here to Attach ]" 
                    : ("Script File: " + nsc.ScriptName + ".h (Drop to Replace)");
                ImGui::Button(dropSlotLabel.c_str(), ImVec2(-1, 32.0f));
                ImGui::PopStyleColor(2);

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        std::filesystem::path path(pathStr);
                        std::string ext = path.extension().string();
                        for (auto& c : ext) c = static_cast<char>(tolower(c));
                        if (ext == ".h" || ext == ".cpp") {
                            std::string scriptName = path.stem().string();
                            nsc.BindByName(scriptName);
                            OVERFLOW_INFO("Script '{0}' vinculado ao componente via Drag & Drop!", scriptName);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::Spacing();

                // 2. Seleção de Script por Lista Suspensa
                const auto& registeredScripts = ScriptRegistry::GetScripts();
                std::vector<std::string> scriptNames;
                for (const auto& [name, info] : registeredScripts) {
                    scriptNames.push_back(name);
                }

                ImGui::Text("Script Class:");
                if (ImGui::BeginCombo("##ScriptClassCombo", nsc.ScriptName.c_str())) {
                    for (int i = 0; i < static_cast<int>(scriptNames.size()); ++i) {
                        bool isSelected = (nsc.ScriptName == scriptNames[i]);
                        if (ImGui::Selectable(scriptNames[i].c_str(), isSelected)) {
                            nsc.BindByName(scriptNames[i]);
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (registeredScripts.find(nsc.ScriptName) != registeredScripts.end()) {
                    ImGui::TextDisabled("%s", registeredScripts.at(nsc.ScriptName).Description.c_str());
                    ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Status: Registered & Ready to Run");
                } else if (nsc.ScriptName != "None" && !nsc.ScriptName.empty()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.25f, 1.0f), "Status: Attached (Compile in Rider or Press F5 for Live-Reload)");
                }

                ImGui::Spacing();

                // 3. Botão para Abrir Script na IDE Configurada
                if (nsc.ScriptName != "None" && !nsc.ScriptName.empty()) {
                    std::string openLabel = "Open " + nsc.ScriptName + ".h in Preferred IDE";
                    if (ImGui::Button(openLabel.c_str(), ImVec2(-1, 26.0f))) {
                        std::vector<std::filesystem::path> searchDirs = {
                            "assets/scripts",
                            "src/Engine/Scene/Scripts",
                            "assets"
                        };
                        bool found = false;
                        for (const auto& dir : searchDirs) {
                            std::filesystem::path p = dir / (nsc.ScriptName + ".h");
                            if (std::filesystem::exists(p)) {
                                EditorPreferences::OpenFileInIDE(p);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            EditorPreferences::OpenFileInIDE("assets/scripts/" + nsc.ScriptName + ".h");
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Remove Script Component", ImVec2(-1, 24.0f))) {
                    removeScript = true;
                }
            }
            ImGui::PopStyleColor();

            if (removeScript) {
                entity.RemoveComponent<NativeScriptComponent>();
            }
        }

        // ---------------------------------------------------------------------
        // Exportação para Entity Archetype (.ovbp) - Overflow Engine
        // ---------------------------------------------------------------------
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save as Entity Archetype (.ovbp)", ImVec2(-1, 26.0f))) {
            std::filesystem::create_directories("assets/archetypes");
            std::string tag = entity.GetComponent<TagComponent>().Tag;
            std::string cleanName = "";
            for (char c : tag) {
                if (isalnum(c) || c == '_' || c == '-') cleanName += c;
                else cleanName += '_';
            }
            std::string path = "assets/archetypes/" + cleanName + ".ovbp";
            if (SceneSerializer::SerializeEntity(entity, path)) {
                OVERFLOW_INFO("Entity Archetype salvo com sucesso: {0}", path);
            }
        }

        // ---------------------------------------------------------------------
        // Botão [+ Add Component] estilo Unity no rodapé do Inspector
        // ---------------------------------------------------------------------
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.60f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.45f, 0.75f, 1.0f));
        if (ImGui::Button("+ Add Component", ImVec2(-1, 28.0f))) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        ImGui::PopStyleColor(2);

        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!entity.HasComponent<MeshComponent>()) {
                if (ImGui::MenuItem("Mesh Renderer (PBR)")) {
                    auto& mesh = entity.AddComponent<MeshComponent>(MeshGeometryType::Cube);
                    mesh.ModelAsset = m_Context->GetSharedCube();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<LightComponent>()) {
                if (ImGui::MenuItem("Light (PBR)")) {
                    entity.AddComponent<LightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<CameraComponent>()) {
                if (ImGui::MenuItem("Camera")) {
                    entity.AddComponent<CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (ImGui::BeginMenu("Effects")) {
                if (!entity.HasComponent<ParticleSystemComponent>()) {
                    if (ImGui::MenuItem("Particle System")) {
                        entity.AddComponent<ParticleSystemComponent>();
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Audio")) {
                if (!entity.HasComponent<AudioSourceComponent>()) {
                    if (ImGui::MenuItem("Audio Source")) {
                        entity.AddComponent<AudioSourceComponent>();
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!entity.HasComponent<AudioListenerComponent>()) {
                    if (ImGui::MenuItem("Audio Listener")) {
                        entity.AddComponent<AudioListenerComponent>();
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Physics")) {
                if (!entity.HasComponent<RigidbodyComponent>()) {
                    if (ImGui::MenuItem("Rigidbody 3D")) {
                        entity.AddComponent<RigidbodyComponent>();
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!entity.HasComponent<ColliderComponent>()) {
                    if (ImGui::MenuItem("Box Collider")) {
                        entity.AddComponent<ColliderComponent>(ColliderType::Box);
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::MenuItem("Sphere Collider")) {
                        entity.AddComponent<ColliderComponent>(ColliderType::Sphere);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scripting")) {
                if (!entity.HasComponent<NativeScriptComponent>()) {
                    for (const auto& [name, info] : ScriptRegistry::GetScripts()) {
                        std::string itemLabel = name + " Script";
                        if (ImGui::MenuItem(itemLabel.c_str())) {
                            entity.AddComponent<NativeScriptComponent>().BindByName(name);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

}
