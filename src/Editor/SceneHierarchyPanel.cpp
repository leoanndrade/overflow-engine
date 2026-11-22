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
    }

    void SceneHierarchyPanel::OnImGuiRender() {
        // ---------------------------------------------------------------------
        // Painel: World Outliner (Nomenclatura e Identidade Propria Overflow Engine)
        // ---------------------------------------------------------------------
        ImGui::Begin("World Outliner");
        m_HierarchyFocused = ImGui::IsWindowFocused();

        // Barra Superior: Busca + Botão [+ Add]
        static char s_SearchFilter[128] = "";
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(availWidth - 75.0f);
        ImGui::InputTextWithHint("##HierarchySearch", "Search Entities...", s_SearchFilter, sizeof(s_SearchFilter));

        ImGui::SameLine();
        if (ImGui::Button("+ Add", ImVec2(68.0f, 0.0f))) {
            ImGui::OpenPopup("HierarchyAddPopup");
        }

        if (ImGui::BeginPopup("HierarchyAddPopup")) {
            if (ImGui::MenuItem("Empty Entity")) {
                auto created = m_Context->CreateEntity("Empty Entity");
                m_SelectionContext = created;
            }
            ImGui::Separator();
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
                ImGui::EndPopup();
            }

            ImGui::EndChild();
        }

        ImGui::End();

        // ---------------------------------------------------------------------
        // Painel: Entity Details (Propriedades & Componentes)
        // ---------------------------------------------------------------------
        ImGui::Begin("Entity Details");

        if (m_SelectionContext) {
            DrawComponents(m_SelectionContext);

            // Drag & Drop direto no Painel de Detalhes (Anexa Scripts, Materiais, Texturas)
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
                        OVERFLOW_INFO("Script '{0}' anexado a entidade '{1}' via Entity Details!", scriptName, m_SelectionContext.GetComponent<TagComponent>().Tag);
                    } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                        if (m_SelectionContext.HasComponent<MeshComponent>()) {
                            m_SelectionContext.GetComponent<MeshComponent>().TextureAsset = Texture2D::Create(path.string());
                            OVERFLOW_INFO("Textura aplicada a entidade '{0}'!", m_SelectionContext.GetComponent<TagComponent>().Tag);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        } else {
            ImGui::Spacing();
            ImGui::TextDisabled("Select an entity from the Scene or Outliner to inspect its components.");
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
                ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "Surface Material");

                // Base Color
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Albedo");
                ImGui::NextColumn();
                ImGui::ColorEdit4("##BaseColor", glm::value_ptr(mesh.Color), ImGuiColorEditFlags_NoInputs);
                ImGui::Columns(1);

                // Texture Slot estilo Unity
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Texture");
                ImGui::NextColumn();

                std::string texLabel = mesh.TextureAsset ? mesh.TextureAsset->GetPath() : "None (Drag Texture Here)";
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
                ImGui::Button(texLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 24.0f));
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        mesh.TextureAsset = Texture2D::Create(pathStr);
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("Demo", ImVec2(40.0f, 24.0f))) {
                    mesh.TextureAsset = Texture2D::Create("assets/textures/uv_checker.bmp");
                }
                ImGui::SameLine();
                if (ImGui::Button("X", ImVec2(24.0f, 24.0f))) {
                    mesh.TextureAsset = nullptr;
                }
                ImGui::Columns(1);

                // PBR Sliders
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Metallic");
                ImGui::NextColumn();
                ImGui::SliderFloat("##Metallic", &mesh.Metallic, 0.0f, 1.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("Roughness");
                ImGui::NextColumn();
                ImGui::SliderFloat("##Roughness", &mesh.Roughness, 0.01f, 1.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("AO");
                ImGui::NextColumn();
                ImGui::SliderFloat("##AO", &mesh.AO, 0.0f, 1.0f, "%.2f");
                ImGui::NextColumn();

                ImGui::Text("Tiling");
                ImGui::NextColumn();
                ImGui::DragFloat("##Tiling", &mesh.Tiling, 0.1f, 0.1f, 20.0f, "%.1f");
                ImGui::Columns(1);

                // Slot de Normal Map (Relevo PBR 3D)
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 85.0f);
                ImGui::Text("Normal Map");
                ImGui::NextColumn();

                std::string normLabel = mesh.NormalMapAsset ? mesh.NormalMapAsset->GetPath() : "None (Drag Normal Map)";
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
                ImGui::Button(normLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 70.0f, 24.0f));
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string pathStr = (const char*)payload->Data;
                        mesh.NormalMapAsset = Texture2D::Create(pathStr);
                        mesh.UseNormalMap = true;
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("Demo##Norm", ImVec2(40.0f, 24.0f))) {
                    mesh.NormalMapAsset = Texture2D::Create("assets/textures/uv_checker.bmp");
                    mesh.UseNormalMap = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("X##Norm", ImVec2(24.0f, 24.0f))) {
                    mesh.NormalMapAsset = nullptr;
                    mesh.UseNormalMap = false;
                }
                ImGui::Columns(1);
                ImGui::Checkbox("Enable Normal Mapping", &mesh.UseNormalMap);

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
