#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Overflow {

    class Log {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// Core Log Macros
#define OVERFLOW_CORE_TRACE(...)    ::Overflow::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define OVERFLOW_CORE_INFO(...)     ::Overflow::Log::GetCoreLogger()->info(__VA_ARGS__)
#define OVERFLOW_CORE_WARN(...)     ::Overflow::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define OVERFLOW_CORE_ERROR(...)    ::Overflow::Log::GetCoreLogger()->error(__VA_ARGS__)
#define OVERFLOW_CORE_CRITICAL(...) ::Overflow::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client Log Macros
#define OVERFLOW_TRACE(...)         ::Overflow::Log::GetClientLogger()->trace(__VA_ARGS__)
#define OVERFLOW_INFO(...)          ::Overflow::Log::GetClientLogger()->info(__VA_ARGS__)
#define OVERFLOW_WARN(...)          ::Overflow::Log::GetClientLogger()->warn(__VA_ARGS__)
#define OVERFLOW_ERROR(...)         ::Overflow::Log::GetClientLogger()->error(__VA_ARGS__)
#define OVERFLOW_CRITICAL(...)      ::Overflow::Log::GetClientLogger()->critical(__VA_ARGS__)
