#pragma once

#include <string>
#include <functional>

struct GLFWwindow;

namespace Engine {

    struct WindowProps {
        std::string Title;
        uint32_t Width;
        uint32_t Height;
        bool VSync;

        WindowProps(const std::string& title = "Custom Engine 3D",
                    uint32_t width = 1280,
                    uint32_t height = 720,
                    bool vsync = true)
            : Title(title), Width(width), Height(height), VSync(vsync) {}
    };

    class Window {
    public:
        using EventCallbackFn = std::function<void()>;

        Window(const WindowProps& props = WindowProps());
        ~Window();

        void OnUpdate();

        inline uint32_t GetWidth() const { return m_Data.Width; }
        inline uint32_t GetHeight() const { return m_Data.Height; }
        inline GLFWwindow* GetNativeWindow() const { return m_Window; }

        void SetVSync(bool enabled);
        bool IsVSync() const;

        bool ShouldClose() const;

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        GLFWwindow* m_Window = nullptr;

        struct WindowData {
            std::string Title;
            uint32_t Width, Height;
            bool VSync;
        };

        WindowData m_Data;
    };

}
