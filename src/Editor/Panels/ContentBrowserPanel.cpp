#include "ContentBrowserPanel.h"
#include "Editor/EditorPreferences.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/Material.h"

#include <imgui.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace Overflow {

    ContentBrowserPanel::ContentBrowserPanel()
        : m_BaseDirectory("assets"), m_CurrentDirectory("assets") {
        if (!std::filesystem::exists(m_BaseDirectory)) {
            std::filesystem::create_directories(m_BaseDirectory);
        }
        std::filesystem::create_directories("assets/scenes");
        std::filesystem::create_directories("assets/scripts");
        std::filesystem::create_directories("assets/shaders");
        std::filesystem::create_directories("assets/materials");
        std::filesystem::create_directories("assets/archetypes");
        std::filesystem::create_directories("assets/textures");
    }

    void ContentBrowserPanel::DrawFolderTree(const std::filesystem::path& currentPath) {
        if (!std::filesystem::exists(currentPath)) return;

        for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
            if (entry.is_directory()) {
                const auto& path = entry.path();
                std::string folderName = path.filename().string();

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (m_CurrentDirectory == path) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                // Verifica se tem subdiretórios
                bool hasSubDirs = false;
                for (const auto& sub : std::filesystem::directory_iterator(path)) {
                    if (sub.is_directory()) {
                        hasSubDirs = true;
                        break;
                    }
                }

                if (!hasSubDirs) flags |= ImGuiTreeNodeFlags_Leaf;

                std::string label = "[Dir] " + folderName;
                bool opened = ImGui::TreeNodeEx(path.string().c_str(), flags, "%s", label.c_str());

                if (ImGui::IsItemClicked()) {
                    m_CurrentDirectory = path;
                }

                if (opened) {
                    if (hasSubDirs) {
                        DrawFolderTree(path);
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    void ContentBrowserPanel::OnImGuiRender(bool* p_open) {
        ImGui::Begin("Content Browser", p_open);

        RenderHeaderToolbar();

        ImGui::Separator();

        // Layout de 2 Colunas (Esquerda: Árvore de Pastas, Direita: Visualização de Arquivos)
        ImGui::Columns(2, "ContentBrowserSplitter", true);
        ImGui::SetColumnWidth(0, 190.0f);

        // Painel Esquerdo: Árvore de Diretórios
        ImGui::BeginChild("FolderTreePane", ImVec2(0, 0), false);
        {
            ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
            if (m_CurrentDirectory == m_BaseDirectory) rootFlags |= ImGuiTreeNodeFlags_Selected;

            bool rootOpened = ImGui::TreeNodeEx("RootAssets", rootFlags, "[Project] Assets");
            if (ImGui::IsItemClicked()) {
                m_CurrentDirectory = m_BaseDirectory;
            }

            if (rootOpened) {
                if (std::filesystem::exists(m_BaseDirectory)) {
                    DrawFolderTree(m_BaseDirectory);
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();

        ImGui::NextColumn();

        // Painel Direito: Conteúdo da Pasta
        ImGui::BeginChild("AssetContentPane", ImVec2(0, -24.0f), false);

        // Coleta entradas ordenadas: pastas primeiro, depois arquivos
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> files;

        std::string search = m_SearchFilter;
        for (auto& c : search) c = static_cast<char>(tolower(c));

        if (std::filesystem::exists(m_CurrentDirectory)) {
            for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
                std::string name = entry.path().filename().string();
                if (!search.empty()) {
                    std::string lower = name;
                    for (auto& c : lower) c = static_cast<char>(tolower(c));
                    if (lower.find(search) == std::string::npos)
                        continue;
                }

                if (entry.is_directory()) directories.push_back(entry);
                else files.push_back(entry);
            }
        }

        std::vector<std::filesystem::directory_entry> allEntries;
        allEntries.insert(allEntries.end(), directories.begin(), directories.end());
        allEntries.insert(allEntries.end(), files.begin(), files.end());

        // Renderiza no modo ativo (Grade ou Lista)
        if (m_ViewMode == ContentBrowserViewMode::List) {
            RenderListView(allEntries);
        } else {
            RenderGridView(allEntries);
        }

        RenderContextMenu();

        ImGui::EndChild();

        // Barra de Status Inferior
        ImGui::Separator();
        int totalItems = static_cast<int>(allEntries.size());
        const char* modeText = (m_ViewMode == ContentBrowserViewMode::List) ? "List View" : "Grid View";
        ImGui::TextDisabled("%d item(s)  |  %s  |  IDE: %s  |  %s", 
                            totalItems, modeText, 
                            EditorPreferences::GetIDEName(EditorPreferences::GetPreferredIDE()),
                            m_CurrentDirectory.string().c_str());

        ImGui::Columns(1);

        RenderModals();

        ImGui::End();
    }

    void ContentBrowserPanel::RenderHeaderToolbar() {
        // 1. Botão de Voltar ao diretório pai
        if (m_CurrentDirectory != m_BaseDirectory) {
            if (ImGui::Button("<-", ImVec2(28.0f, 22.0f))) {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
            }
            ImGui::SameLine();
        }

        // 2. Breadcrumbs Clicáveis (Navegação hierárquica rápida)
        std::filesystem::path rel = std::filesystem::relative(m_CurrentDirectory, m_BaseDirectory);
        if (ImGui::SmallButton("Assets")) {
            m_CurrentDirectory = m_BaseDirectory;
        }
        ImGui::SameLine();

        if (rel != ".") {
            std::filesystem::path accum = m_BaseDirectory;
            for (const auto& part : rel) {
                accum /= part;
                ImGui::TextDisabled(">");
                ImGui::SameLine();
                if (ImGui::SmallButton(part.string().c_str())) {
                    m_CurrentDirectory = accum;
                }
                ImGui::SameLine();
            }
        }

        // 3. Campo de Busca com filtro (Sem botão + Add, criação via Botão Direito ou Menu Assets)

        // 4. Campo de Busca com filtro
        float searchPos = ImGui::GetWindowContentRegionMax().x - 390.0f;
        if (searchPos > ImGui::GetCursorPosX() + 10.0f) {
            ImGui::SetCursorPosX(searchPos);
        }
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputTextWithHint("##AssetSearch", "Search...", m_SearchFilter, sizeof(m_SearchFilter));

        ImGui::SameLine();

        // 5. Botões de Alternância Rápida: [Grid] e [List]
        if (m_ViewMode == ContentBrowserViewMode::Grid) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.65f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.22f, 0.6f));
        }
        if (ImGui::Button("Grid", ImVec2(40.0f, 22.0f))) {
            m_ViewMode = ContentBrowserViewMode::Grid;
            if (m_ThumbnailSize < 48.0f) m_ThumbnailSize = 78.0f;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (m_ViewMode == ContentBrowserViewMode::List) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.65f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.22f, 0.6f));
        }
        if (ImGui::Button("List", ImVec2(40.0f, 22.0f))) {
            m_ViewMode = ContentBrowserViewMode::List;
            m_ThumbnailSize = 28.0f;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // 6. Slider de Zoom: Ao tirar totalmente o zoom, vira LISTAS automaticamente!
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("##Zoom", &m_ThumbnailSize, 28.0f, 130.0f, "Zoom: %.0f")) {
            // Se tirar totalmente o zoom (<= 34.0f), ativa visualização de Lista automaticamente
            if (m_ThumbnailSize <= 34.0f) {
                m_ViewMode = ContentBrowserViewMode::List;
            } else {
                m_ViewMode = ContentBrowserViewMode::Grid;
            }
        }
    }

    void ContentBrowserPanel::RenderGridView(const std::vector<std::filesystem::directory_entry>& entries) {
        float padding = 10.0f;
        float cellSize = m_ThumbnailSize + padding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        for (const auto& directoryEntry : entries) {
            const auto& path = directoryEntry.path();
            std::string filenameString = path.filename().string();
            bool isDirectory = directoryEntry.is_directory();

            ImGui::PushID(filenameString.c_str());

            std::string ext = path.extension().string();
            for (auto& c : ext) c = static_cast<char>(tolower(c));

            // Ícones e cores especializadas para cada categoria de arquivo da Overflow Engine
            std::string badge = "[File]";
            ImVec4 buttonColor = ImVec4(0.16f, 0.17f, 0.20f, 0.7f);

            if (isDirectory) {
                badge = "[Dir]";
                buttonColor = ImVec4(0.16f, 0.32f, 0.54f, 0.7f);
            } else if (ext == ".h" || ext == ".cpp" || ext == ".hpp") {
                badge = "[C++]";
                buttonColor = ImVec4(0.15f, 0.42f, 0.65f, 0.8f);
            } else if (ext == ".glsl" || ext == ".vert" || ext == ".frag") {
                badge = "[Shader]";
                buttonColor = ImVec4(0.48f, 0.22f, 0.62f, 0.8f);
            } else if (ext == ".ovbp" || ext == ".ovar" || ext == ".archetype" || ext == ".prefab") {
                badge = "[Archetype]";
                buttonColor = ImVec4(0.70f, 0.45f, 0.12f, 0.8f);
            } else if (ext == ".ovmat" || ext == ".mat") {
                badge = "[Mat]";
                buttonColor = ImVec4(0.18f, 0.55f, 0.45f, 0.8f);
            } else if (ext == ".ovscene" || ext == ".scene" || ext == ".overflow") {
                badge = "[Scene]";
                buttonColor = ImVec4(0.55f, 0.25f, 0.40f, 0.8f);
            } else if (ext == ".obj") {
                badge = "[Mesh]";
                buttonColor = ImVec4(0.60f, 0.35f, 0.15f, 0.8f);
            } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                badge = "[Tex]";
                buttonColor = ImVec4(0.20f, 0.52f, 0.28f, 0.8f);
            } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
                badge = "[Audio]";
                buttonColor = ImVec4(0.60f, 0.25f, 0.50f, 0.8f);
            }

            // Exibição em grade compacta
            std::string buttonLabel = badge + "\n" + filenameString;
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);

            if (ImGui::Button(buttonLabel.c_str(), ImVec2(m_ThumbnailSize, m_ThumbnailSize))) {
                m_SelectedPath = path;
                if (m_AssetSelectedCallback) {
                    m_AssetSelectedCallback(m_SelectedPath);
                }
            }

            // Duplo clique detectado no item
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                HandleItemDoubleClick(path, isDirectory);
            }

            // Tooltip rica
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", filenameString.c_str());
                if (!isDirectory && std::filesystem::exists(path)) {
                    auto fsize = std::filesystem::file_size(path);
                    if (fsize < 1024) ImGui::TextDisabled("Tamanho: %llu B", fsize);
                    else if (fsize < 1024 * 1024) ImGui::TextDisabled("Tamanho: %.1f KB", fsize / 1024.0f);
                    else ImGui::TextDisabled("Tamanho: %.2f MB", fsize / (1024.0f * 1024.0f));
                }
                ImGui::TextDisabled("Caminho: %s", path.string().c_str());
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Duplo clique para abrir na IDE");
                ImGui::EndTooltip();
            }

            // Drag & Drop Source
            if (!isDirectory && ImGui::BeginDragDropSource()) {
                std::string itemPath = path.string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);
                ImGui::Text("Asset: %s", filenameString.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PopStyleColor();
            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    void ContentBrowserPanel::RenderListView(const std::vector<std::filesystem::directory_entry>& entries) {
        // Tabela profissional detalhada (estilo Unity 6 List / Windows Explorer Details)
        static ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg 
                                          | ImGuiTableFlags_BordersInnerV 
                                          | ImGuiTableFlags_ScrollY 
                                          | ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("ContentBrowserListTable", 4, tableFlags, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 4.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableHeadersRow();

            for (const auto& directoryEntry : entries) {
                const auto& path = directoryEntry.path();
                std::string filename = path.filename().string();
                bool isDirectory = directoryEntry.is_directory();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                std::string ext = path.extension().string();
                for (auto& c : ext) c = static_cast<char>(tolower(c));

                // Ícone e tipo legível
                std::string icon = "[File] ";
                std::string typeStr = "Asset File";
                ImVec4 textColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

                if (isDirectory) {
                    icon = "[Dir] ";
                    typeStr = "Folder";
                    textColor = ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
                } else if (ext == ".h" || ext == ".cpp" || ext == ".hpp") {
                    icon = "[C++] ";
                    typeStr = "C++ Script (EntityBehavior)";
                    textColor = ImVec4(0.40f, 0.80f, 1.0f, 1.0f);
                } else if (ext == ".glsl" || ext == ".vert" || ext == ".frag") {
                    icon = "[Shader] ";
                    typeStr = "GLSL PBR Shader";
                    textColor = ImVec4(0.85f, 0.55f, 0.95f, 1.0f);
                } else if (ext == ".ovbp" || ext == ".ovar" || ext == ".archetype" || ext == ".prefab") {
                    icon = "[Archetype] ";
                    typeStr = "Entity Archetype";
                    textColor = ImVec4(1.0f, 0.75f, 0.35f, 1.0f);
                } else if (ext == ".ovmat" || ext == ".mat") {
                    icon = "[Mat] ";
                    typeStr = "Material Asset";
                    textColor = ImVec4(0.40f, 0.90f, 0.70f, 1.0f);
                } else if (ext == ".ovscene" || ext == ".scene" || ext == ".overflow") {
                    icon = "[Scene] ";
                    typeStr = "Overflow Scene";
                    textColor = ImVec4(0.95f, 0.50f, 0.75f, 1.0f);
                } else if (ext == ".obj") {
                    icon = "[Mesh] ";
                    typeStr = "3D Wavefront Mesh";
                    textColor = ImVec4(0.95f, 0.65f, 0.40f, 1.0f);
                } else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                    icon = "[Tex] ";
                    typeStr = "Texture Image";
                    textColor = ImVec4(0.50f, 0.90f, 0.50f, 1.0f);
                }

                // Nome com linha selecionável
                bool selected = (m_SelectedPath == path);
                std::string label = icon + filename;
                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_SelectedPath = path;
                    if (m_AssetSelectedCallback) {
                        m_AssetSelectedCallback(m_SelectedPath);
                    }
                }

                // Duplo clique na linha da lista
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    HandleItemDoubleClick(path, isDirectory);
                }

                // Drag & Drop a partir da linha
                if (!isDirectory && ImGui::BeginDragDropSource()) {
                    std::string itemPath = path.string();
                    ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);
                    ImGui::Text("Asset: %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }

                // Coluna 2: Tipo
                ImGui::TableNextColumn();
                ImGui::TextColored(textColor, "%s", typeStr.c_str());

                // Coluna 3: Tamanho
                ImGui::TableNextColumn();
                if (!isDirectory && std::filesystem::exists(path)) {
                    auto fsize = std::filesystem::file_size(path);
                    if (fsize < 1024) ImGui::TextDisabled("%llu B", fsize);
                    else if (fsize < 1024 * 1024) ImGui::TextDisabled("%.1f KB", fsize / 1024.0f);
                    else ImGui::TextDisabled("%.2f MB", fsize / (1024.0f * 1024.0f));
                } else {
                    ImGui::TextDisabled("-");
                }

                // Coluna 4: Ação rápida
                ImGui::TableNextColumn();
                if (!isDirectory) {
                    ImGui::PushID(filename.c_str());
                    if (ImGui::SmallButton("Edit")) {
                        EditorPreferences::OpenFileInIDE(path);
                    }
                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }
    }

    void ContentBrowserPanel::RenderContextMenu() {
        if (ImGui::BeginPopupContextWindow("ContentBrowserGridContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::BeginMenu("+ Create")) {
                if (ImGui::MenuItem("Material Asset (.ovmat)...")) m_ShowCreateMaterialModal = true;
                if (ImGui::MenuItem("C++ Script (OBehaviour)...")) m_ShowCreateScriptModal = true;
                if (ImGui::MenuItem("GLSL PBR Shader File (.glsl)...")) m_ShowCreateShaderModal = true;
                if (ImGui::MenuItem("Entity Archetype (.ovbp)...")) m_ShowCreateArchetypeModal = true;
                ImGui::Separator();
                if (ImGui::MenuItem("New Folder...")) m_ShowNewFolderModal = true;
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Show in Windows Explorer")) {
#ifdef _WIN32
                ShellExecuteA(NULL, "open", std::filesystem::absolute(m_CurrentDirectory).string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
            }

            ImGui::EndPopup();
        }
    }

    void ContentBrowserPanel::HandleItemDoubleClick(const std::filesystem::path& path, bool isDirectory) {
        if (isDirectory) {
            m_CurrentDirectory /= path.filename();
            return;
        }

        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));

        // 1. Arquivos de Cena (.ovscene, .scene)
        if (ext == ".ovscene" || ext == ".scene" || ext == ".overflow") {
            if (m_OpenSceneCallback) {
                m_OpenSceneCallback(path.string());
                return;
            }
        }

        // 2. Arquétipos (.ovbp, .archetype, .prefab)
        if (ext == ".ovbp" || ext == ".archetype" || ext == ".prefab") {
            if (m_InstantiateArchetypeCallback) {
                m_InstantiateArchetypeCallback(path.string());
                return;
            }
        }

        // 3. Arquivos de Código, Shaders, Materiais, Configurações
        // Abre diretamente na IDE preferida configurada (JetBrains Rider, Visual Studio, VSCode)
        EditorPreferences::OpenFileInIDE(path);
    }

    void ContentBrowserPanel::RenderModals() {
        // Modal: Create C++ Script
        if (m_ShowCreateScriptModal) {
            ImGui::OpenPopup("Create C++ Script (EntityBehavior)");
            m_ShowCreateScriptModal = false;
        }

        if (ImGui::BeginPopupModal("Create C++ Script (EntityBehavior)", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Nome da Classe do Script C++:");
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("##NewScriptName", m_NewScriptName, sizeof(m_NewScriptName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            ImGui::TextDisabled("Sera criado em: %s", m_CurrentDirectory.string().c_str());
            ImGui::Spacing();

            if (ImGui::Button("Create Script", ImVec2(120.0f, 26.0f)) || enterPressed) {
                CreateScriptFile(m_NewScriptName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80.0f, 26.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: Create GLSL Shader
        if (m_ShowCreateShaderModal) {
            ImGui::OpenPopup("Create GLSL PBR Shader");
            m_ShowCreateShaderModal = false;
        }

        if (ImGui::BeginPopupModal("Create GLSL PBR Shader", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Nome do Arquivo de Shader (.glsl):");
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("##NewShaderName", m_NewShaderName, sizeof(m_NewShaderName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            ImGui::TextDisabled("Sera criado em: %s", m_CurrentDirectory.string().c_str());
            ImGui::Spacing();

            if (ImGui::Button("Create Shader", ImVec2(120.0f, 26.0f)) || enterPressed) {
                CreateShaderFile(m_NewShaderName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80.0f, 26.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: Create Entity Archetype (.ovbp)
        if (m_ShowCreateArchetypeModal) {
            ImGui::OpenPopup("Create Entity Archetype");
            m_ShowCreateArchetypeModal = false;
        }

        if (ImGui::BeginPopupModal("Create Entity Archetype", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Nome do Arquétipo (.ovbp):");
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("##NewArchetypeName", m_NewArchetypeName, sizeof(m_NewArchetypeName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (ImGui::Button("Create Archetype", ImVec2(130.0f, 26.0f)) || enterPressed) {
                CreateArchetypeFile(m_NewArchetypeName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80.0f, 26.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: Create Material (.ovmat)
        if (m_ShowCreateMaterialModal) {
            ImGui::OpenPopup("Create Material Asset");
            m_ShowCreateMaterialModal = false;
        }

        if (ImGui::BeginPopupModal("Create Material Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Nome do Material (.ovmat):");
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("##NewMaterialName", m_NewMaterialName, sizeof(m_NewMaterialName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (ImGui::Button("Create Material", ImVec2(130.0f, 26.0f)) || enterPressed) {
                CreateMaterialFile(m_NewMaterialName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80.0f, 26.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: New Folder
        if (m_ShowNewFolderModal) {
            ImGui::OpenPopup("New Folder");
            m_ShowNewFolderModal = false;
        }

        if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Nome da Pasta:");
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("##NewFolderName", m_NewFolderName, sizeof(m_NewFolderName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (ImGui::Button("Create Folder", ImVec2(120.0f, 26.0f)) || enterPressed) {
                std::filesystem::create_directories(m_CurrentDirectory / m_NewFolderName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80.0f, 26.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void ContentBrowserPanel::CreateScriptFile(const std::string& name) {
        if (name.empty()) return;
        std::string valid = "";
        for (char c : name) {
            if (isalnum(c) || c == '_') valid += c;
        }
        if (valid.empty()) valid = "MyBehavior";

        std::filesystem::path filePath = m_CurrentDirectory / (valid + ".h");
        if (std::filesystem::exists(filePath)) {
            OVERFLOW_WARN("Script ja existe: {0}", filePath.string());
            EditorPreferences::OpenFileInIDE(filePath);
            return;
        }

        std::ofstream fout(filePath);
        if (fout.is_open()) {
            fout << "#pragma once\n\n"
                 << "#include \"Engine/Scene/ScriptableEntity.h\"\n"
                 << "#include \"Engine/Scene/Component.h\"\n"
                 << "#include \"Engine/Core/Input.h\"\n"
                 << "#include \"Engine/Core/KeyCodes.h\"\n"
                 << "#include \"Engine/Core/Log.h\"\n\n"
                 << "namespace Overflow\n{\n"
                 << "    class " << valid << " : public OBehaviour\n    {\n"
                 << "    public:\n"
                 << "        float MovementSpeed = 5.0f;\n\n"
                 << "    protected:\n"
                 << "        void OnInit() override\n        {\n"
                 << "            OVERFLOW_INFO(\"" << valid << " inicializado no mundo!\");\n"
                 << "        }\n\n"
                 << "        void OnEnable() override\n        {\n"
                 << "            OVERFLOW_INFO(\"" << valid << " habilitado!\");\n"
                 << "        }\n\n"
                 << "        void OnTick(Timestep ts) override\n        {\n"
                 << "            if (!HasComponent<TransformComponent>())\n            {\n"
                 << "                return;\n            }\n"
                 << "            auto& tc = GetComponent<TransformComponent>();\n"
                 << "            // Logica de gameplay e fisica no OnTick:\n"
                 << "        }\n\n"
                 << "        void OnDisable() override\n        {\n"
                 << "            OVERFLOW_INFO(\"" << valid << " desabilitado!\");\n"
                 << "        }\n\n"
                 << "        void OnDestroy() override\n        {\n"
                 << "            OVERFLOW_INFO(\"" << valid << " destruido!\");\n"
                 << "        }\n"
                 << "    };\n"
                 << "}\n";
            fout.close();
            OVERFLOW_INFO("Script C++ criado: {0}", filePath.string());

            // Registra automaticamente no master header UserScripts.h
            std::filesystem::path masterHeader = "src/Engine/Scene/UserScripts.h";
            if (std::filesystem::exists(masterHeader)) {
                std::string relPath = "../../../" + filePath.string();
                std::replace(relPath.begin(), relPath.end(), '\\', '/');

                std::ifstream fin(masterHeader);
                std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                fin.close();

                if (content.find(filePath.filename().string()) == std::string::npos) {
                    std::ofstream appendMaster(masterHeader, std::ios::app);
                    appendMaster << "\n#if __has_include(\"" << relPath << "\")\n#include \"" << relPath << "\"\n#endif\n";
                    appendMaster.close();
                }
            }

            // Registra centralizadamente no UserScriptsRegistry.cpp (script 100% puro para o usuario)
            std::filesystem::path registryCpp = "src/Engine/Scene/UserScriptsRegistry.cpp";
            if (std::filesystem::exists(registryCpp)) {
                std::ifstream fin(registryCpp);
                std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                fin.close();

                std::string regCall = "ScriptRegistry::Register<" + valid + ">(\"" + valid + "\", \"Custom OBehaviour\");";
                if (content.find(regCall) == std::string::npos) {
                    size_t pos = content.rfind("OVERFLOW_INFO");
                    if (pos != std::string::npos) {
                        content.insert(pos, "        " + regCall + "\n\n        ");
                        std::ofstream foutReg(registryCpp);
                        foutReg << content;
                        foutReg.close();
                    }
                }
            }

            // Abre automaticamente no IDE preferido
            EditorPreferences::OpenFileInIDE(filePath);
        }
    }

    void ContentBrowserPanel::CreateShaderFile(const std::string& name) {
        if (name.empty()) return;
        std::string clean = name;
        if (clean.find(".glsl") == std::string::npos) clean += ".glsl";

        std::filesystem::path filePath = m_CurrentDirectory / clean;
        if (std::filesystem::exists(filePath)) {
            EditorPreferences::OpenFileInIDE(filePath);
            return;
        }

        std::ofstream fout(filePath);
        if (fout.is_open()) {
            fout << "// -----------------------------------------------------------------------------\n"
                 << "// Overflow Engine - Custom PBR Shader\n"
                 << "// -----------------------------------------------------------------------------\n\n"
                 << "#type vertex\n"
                 << "#version 460 core\n\n"
                 << "layout (location = 0) in vec3 a_Position;\n"
                 << "layout (location = 1) in vec3 a_Normal;\n"
                 << "layout (location = 2) in vec2 a_TexCoord;\n\n"
                 << "uniform mat4 u_ViewProjection;\n"
                 << "uniform mat4 u_Transform;\n\n"
                 << "out vec3 v_FragPos;\n"
                 << "out vec3 v_Normal;\n"
                 << "out vec2 v_TexCoord;\n\n"
                 << "void main() {\n"
                 << "    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);\n"
                 << "    v_FragPos = worldPos.xyz;\n"
                 << "    v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;\n"
                 << "    v_TexCoord = a_TexCoord;\n"
                 << "    gl_Position = u_ViewProjection * worldPos;\n"
                 << "}\n\n"
                 << "#type fragment\n"
                 << "#version 460 core\n\n"
                 << "in vec3 v_FragPos;\n"
                 << "in vec3 v_Normal;\n"
                 << "in vec2 v_TexCoord;\n\n"
                 << "uniform vec4 u_Color;\n"
                 << "uniform float u_Roughness;\n"
                 << "uniform float u_Metallic;\n"
                 << "uniform vec3 u_CameraPosition;\n\n"
                 << "out vec4 FragColor;\n\n"
                 << "void main() {\n"
                 << "    vec3 N = normalize(v_Normal);\n"
                 << "    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));\n"
                 << "    float diff = max(dot(N, lightDir), 0.15);\n"
                 << "    vec3 diffuse = diff * u_Color.rgb;\n"
                 << "    FragColor = vec4(diffuse, u_Color.a);\n"
                 << "}\n";
            fout.close();
            OVERFLOW_INFO("Shader GLSL criado: {0}", filePath.string());

            EditorPreferences::OpenFileInIDE(filePath);
        }
    }

    void ContentBrowserPanel::CreateMaterialFile(const std::string& name) {
        if (name.empty()) return;
        std::string clean = name;
        if (clean.find(".ovmat") == std::string::npos) clean += ".ovmat";

        std::filesystem::path filePath = m_CurrentDirectory / clean;
        auto mat = Material::Create(std::filesystem::path(name).stem().string());
        mat->Save(filePath.string());
        OVERFLOW_INFO("Material criado: {0}", filePath.string());
    }

    void ContentBrowserPanel::CreateArchetypeFile(const std::string& name) {
        if (name.empty()) return;
        std::string clean = name;
        if (clean.find(".ovbp") == std::string::npos) clean += ".ovbp";

        std::filesystem::path filePath = m_CurrentDirectory / clean;
        std::ofstream fout(filePath);
        if (fout.is_open()) {
            fout << "# Overflow Engine Entity Archetype (.ovbp)\n"
                 << "Archetype: " << clean << "\n"
                 << "Entities:\n"
                 << "  - Entity: 1\n"
                 << "    TagComponent:\n"
                 << "      Tag: " << clean << "\n"
                 << "    TransformComponent:\n"
                 << "      Translation: [0, 0, 0]\n"
                 << "      Rotation: [0, 0, 0]\n"
                 << "      Scale: [1, 1, 1]\n"
                 << "    MeshComponent:\n"
                 << "      GeometryType: 0\n"
                 << "      Color: [1, 1, 1, 1]\n";
            fout.close();
            OVERFLOW_INFO("Entity Archetype criado: {0}", filePath.string());
            EditorPreferences::OpenFileInIDE(filePath);
        }
    }

}
