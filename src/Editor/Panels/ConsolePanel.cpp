#include "ConsolePanel.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <iomanip>
#include <sstream>

namespace Overflow {

    std::shared_ptr<EditorConsoleSink> ConsolePanel::s_Sink = std::make_shared<EditorConsoleSink>();

    void EditorConsoleSink::sink_it_(const spdlog::details::log_msg& msg) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_Messages.size() >= m_MaxMessages) {
            m_Messages.erase(m_Messages.begin());
        }

        auto time = std::chrono::system_clock::to_time_t(msg.time);
        std::tm tm;
        localtime_s(&tm, &time);
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M:%S");

        ConsoleLogMessage logMsg;
        logMsg.Message = std::string(msg.payload.data(), msg.payload.size());
        logMsg.Level = msg.level;
        logMsg.TimeStr = ss.str();

        m_Messages.push_back(logMsg);
    }

    ConsolePanel::ConsolePanel() {
        auto coreLogger = Log::GetCoreLogger();
        if (coreLogger) {
            coreLogger->sinks().push_back(s_Sink);
        }
        auto clientLogger = Log::GetClientLogger();
        if (clientLogger) {
            clientLogger->sinks().push_back(s_Sink);
        }
    }

    void ConsolePanel::OnImGuiRender(bool* p_open) {
        ImGui::Begin("Console", p_open);

        const auto& messages = s_Sink->GetMessages();

        // Contagem de mensagens por categoria
        int infoCount = 0, warnCount = 0, errorCount = 0;
        for (const auto& msg : messages) {
            if (msg.Level == spdlog::level::info || msg.Level == spdlog::level::trace) infoCount++;
            else if (msg.Level == spdlog::level::warn) warnCount++;
            else if (msg.Level == spdlog::level::err || msg.Level == spdlog::level::critical) errorCount++;
        }

        // Barra de Ferramentas estilo Unity Console
        if (ImGui::Button("Clear", ImVec2(55.0f, 22.0f))) {
            s_Sink->Clear();
            m_SelectedMessage = -1;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Clear on Play", &m_ClearOnPlay);

        ImGui::SameLine();
        ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputTextWithHint("##ConsoleSearch", "Filter...", m_SearchFilter, sizeof(m_SearchFilter));

        // Botões de filtro com contadores no canto direito
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 240.0f);

        auto drawFilterBtn = [](const char* label, bool& state, ImVec4 activeColor) {
            if (state) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
            else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.16f, 0.18f, 0.6f));
            if (ImGui::Button(label)) state = !state;
            ImGui::PopStyleColor();
            ImGui::SameLine();
        };

        std::string infoLabel = "[i] " + std::to_string(infoCount);
        std::string warnLabel = "[!] " + std::to_string(warnCount);
        std::string errLabel = "[X] " + std::to_string(errorCount);

        drawFilterBtn(infoLabel.c_str(), m_ShowInfo, ImVec4(0.20f, 0.40f, 0.70f, 0.9f));
        drawFilterBtn(warnLabel.c_str(), m_ShowWarnings, ImVec4(0.75f, 0.55f, 0.15f, 0.9f));
        drawFilterBtn(errLabel.c_str(), m_ShowErrors, ImVec4(0.75f, 0.20f, 0.20f, 0.9f));

        ImGui::NewLine();
        ImGui::Separator();

        // Layout de 2 Painéis estilo Unity: Painel Superior (Lista), Painel Inferior (Detalhe da Mensagem Selecionada)
        float availableHeight = ImGui::GetContentRegionAvail().y;
        float listHeight = availableHeight > 120.0f ? (availableHeight * 0.68f) : availableHeight;

        ImGui::BeginChild("LogListRegion", ImVec2(0, listHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::string searchStr = m_SearchFilter;
        for (auto& c : searchStr) c = static_cast<char>(tolower(c));

        int index = 0;
        for (const auto& logMsg : messages) {
            bool isInfo = (logMsg.Level == spdlog::level::info || logMsg.Level == spdlog::level::trace);
            bool isWarn = (logMsg.Level == spdlog::level::warn);
            bool isError = (logMsg.Level == spdlog::level::err || logMsg.Level == spdlog::level::critical);

            if (isInfo && !m_ShowInfo) { index++; continue; }
            if (isWarn && !m_ShowWarnings) { index++; continue; }
            if (isError && !m_ShowErrors) { index++; continue; }

            if (!searchStr.empty()) {
                std::string msgLower = logMsg.Message;
                for (auto& c : msgLower) c = static_cast<char>(tolower(c));
                if (msgLower.find(searchStr) == std::string::npos) {
                    index++;
                    continue;
                }
            }

            ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            const char* levelTag = "[INFO]";

            if (isWarn) {
                color = ImVec4(1.0f, 0.75f, 0.20f, 1.0f);
                levelTag = "[WARN]";
            } else if (isError) {
                color = ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
                levelTag = "[ERR] ";
            }

            bool isSelected = (m_SelectedMessage == index);
            std::string itemID = "##log_" + std::to_string(index);

            // Zebra striping de linhas
            if (index % 2 == 0) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13f, 0.14f, 0.16f, 0.6f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.11f, 0.115f, 0.13f, 0.6f));
            }

            if (ImGui::Selectable(itemID.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_SelectedMessage = index;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(10.0f);
            ImGui::TextDisabled("[%s]", logMsg.TimeStr.c_str());
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", levelTag);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", logMsg.Message.c_str());

            index++;
        }

        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // Separador e Painel Inferior de Detalhes
        ImGui::Separator();
        ImGui::BeginChild("LogDetailRegion", ImVec2(0, 0), false);

        if (m_SelectedMessage >= 0 && m_SelectedMessage < static_cast<int>(messages.size())) {
            const auto& selected = messages[m_SelectedMessage];
            ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "Timestamp: %s", selected.TimeStr.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", selected.Message.c_str());
        } else {
            ImGui::TextDisabled("Select a log message to view its full details.");
        }

        ImGui::EndChild();

        ImGui::End();
    }

}
