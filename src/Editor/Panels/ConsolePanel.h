#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

namespace Overflow {

    struct ConsoleLogMessage {
        std::string Message;
        spdlog::level::level_enum Level;
        std::string TimeStr;
    };

    class EditorConsoleSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        EditorConsoleSink(size_t maxMessages = 1000)
            : m_MaxMessages(maxMessages) {}

        const std::vector<ConsoleLogMessage>& GetMessages() const { return m_Messages; }
        void Clear() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Messages.clear();
        }

    protected:
        virtual void sink_it_(const spdlog::details::log_msg& msg) override;
        virtual void flush_() override {}

    private:
        size_t m_MaxMessages;
        std::vector<ConsoleLogMessage> m_Messages;
        mutable std::mutex m_Mutex;
    };

    class ConsolePanel {
    public:
        ConsolePanel();
        ~ConsolePanel() = default;

        void OnImGuiRender(bool* p_open = nullptr);

        static std::shared_ptr<EditorConsoleSink> GetConsoleSink() { return s_Sink; }

    private:
        static std::shared_ptr<EditorConsoleSink> s_Sink;

        bool m_ShowInfo = true;
        bool m_ShowWarnings = true;
        bool m_ShowErrors = true;
        bool m_AutoScroll = true;
        bool m_ClearOnPlay = true;
        int m_SelectedMessage = -1;
        char m_SearchFilter[128] = "";
    };

}
