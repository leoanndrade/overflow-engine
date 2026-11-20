#include "Window.h"
#include "Log.h"

#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Overflow {

    static uint8_t s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description) {
        OVERFLOW_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Window::Window(const WindowProps& props) {
        Init(props);
    }

    Window::~Window() {
        Shutdown();
    }

    void Window::Init(const WindowProps& props) {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;
        m_Data.VSync = props.VSync;

        OVERFLOW_CORE_INFO("Iniciando janela: {0} ({1}x{2})", props.Title, props.Width, props.Height);

        if (s_GLFWWindowCount == 0) {
            int success = glfwInit();
            if (!success) {
                OVERFLOW_CORE_CRITICAL("Falha ao inicializar GLFW!");
                return;
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // Configuração OpenGL 4.6 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        #if defined(__APPLE__)
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        #endif

        m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
        ++s_GLFWWindowCount;

        if (!m_Window) {
            OVERFLOW_CORE_CRITICAL("Falha ao criar janela GLFW!");
            return;
        }

        glfwMakeContextCurrent(m_Window);

        // Inicializar GLAD com o loader do GLFW
        int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
        if (version == 0) {
            OVERFLOW_CORE_CRITICAL("Falha ao inicializar GLAD / carregar funcoes OpenGL!");
            return;
        }

        OVERFLOW_CORE_INFO("OpenGL Renderizador: {0}", (const char*)glGetString(GL_RENDERER));
        OVERFLOW_CORE_INFO("OpenGL Versao:       {0}", (const char*)glGetString(GL_VERSION));
        OVERFLOW_CORE_INFO("OpenGL Vendor:       {0}", (const char*)glGetString(GL_VENDOR));

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(m_Data.VSync);

        // Callback de redimensionamento de janela
        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;
            glViewport(0, 0, width, height);
        });
    }

    void Window::Shutdown() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            --s_GLFWWindowCount;
        }

        if (s_GLFWWindowCount == 0) {
            glfwTerminate();
        }
    }

    void Window::OnUpdate() {
        glfwPollEvents();
        glfwSwapBuffers(m_Window);
    }

    void Window::SetVSync(bool enabled) {
        if (enabled) {
            glfwSwapInterval(1);
        } else {
            glfwSwapInterval(0);
        }
        m_Data.VSync = enabled;
    }

    bool Window::IsVSync() const {
        return m_Data.VSync;
    }

    bool Window::ShouldClose() const {
        return glfwWindowShouldClose(m_Window);
    }

}
