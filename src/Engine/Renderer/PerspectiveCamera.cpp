#include "PerspectiveCamera.h"
#include "Engine/Core/Input.h"

#include <algorithm>

namespace Overflow {

    PerspectiveCamera::PerspectiveCamera(float fov, float aspectRatio, float nearClip, float farClip)
        : m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip) {
        RecalculateProjection();
        RecalculateVectors();
    }

    void PerspectiveCamera::SetViewportSize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

    void PerspectiveCamera::RecalculateProjection() {
        m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
        m_ViewProjectionMatrix = m_Projection * m_ViewMatrix;
    }

    void PerspectiveCamera::RecalculateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Forward = glm::normalize(front);

        m_Right = glm::normalize(glm::cross(m_Forward, m_WorldUp));
        m_Up    = glm::normalize(glm::cross(m_Right, m_Forward));

        RecalculateViewMatrix();
    }

    void PerspectiveCamera::RecalculateViewMatrix() {
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
        m_ViewProjectionMatrix = m_Projection * m_ViewMatrix;
    }

    void PerspectiveCamera::OnUpdate(Timestep ts) {
        // Controle de câmera interativo de voo livre ao segurar Botão Direito (Flycam Unreal / Unity)
        if (Input::IsMouseButtonPressed(Mouse::ButtonRight)) {
            glm::vec2 mousePos = Input::GetMousePosition();

            if (m_FirstMouse) {
                m_LastMousePosition = mousePos;
                m_FirstMouse = false;
                return; // Evita qualquer tranco ou salto no frame inicial
            }

            float xOffset = mousePos.x - m_LastMousePosition.x;
            float yOffset = m_LastMousePosition.y - mousePos.y;
            m_LastMousePosition = mousePos;

            xOffset *= m_MouseSensitivity;
            yOffset *= m_MouseSensitivity;

            m_Yaw   += xOffset;
            m_Pitch += yOffset;
            m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

            RecalculateVectors();

            // Movimentação de voo (WASD, QE, Shift)
            float speed = m_MoveSpeed;
            if (Input::IsKeyPressed(Key::LeftShift)) {
                speed *= 2.5f; // Boost
            }

            float dt = ts.GetSeconds();

            if (Input::IsKeyPressed(Key::W))
                m_Position += m_Forward * (speed * dt);
            if (Input::IsKeyPressed(Key::S))
                m_Position -= m_Forward * (speed * dt);
            if (Input::IsKeyPressed(Key::A))
                m_Position -= m_Right * (speed * dt);
            if (Input::IsKeyPressed(Key::D))
                m_Position += m_Right * (speed * dt);
            if (Input::IsKeyPressed(Key::E) || Input::IsKeyPressed(Key::Space))
                m_Position += m_WorldUp * (speed * dt);
            if (Input::IsKeyPressed(Key::Q) || Input::IsKeyPressed(Key::LeftControl))
                m_Position -= m_WorldUp * (speed * dt);

            m_FocalPoint = m_Position + m_Forward * m_Distance;
            RecalculateViewMatrix();
        } else {
            m_FirstMouse = true;
        }
    }

    void PerspectiveCamera::Pan(const glm::vec2& delta) {
        float panSpeed = 0.0035f * std::max(1.0f, m_Distance);
        m_Position -= m_Right * (delta.x * panSpeed);
        m_Position += m_Up * (delta.y * panSpeed);
        m_FocalPoint -= m_Right * (delta.x * panSpeed);
        m_FocalPoint += m_Up * (delta.y * panSpeed);
        RecalculateViewMatrix();
    }

    void PerspectiveCamera::Orbit(const glm::vec2& delta) {
        float xOffset = delta.x * m_MouseSensitivity;
        float yOffset = -delta.y * m_MouseSensitivity; // Corrigido: mouse para cima orbita a visão para cima

        m_Yaw += xOffset;
        m_Pitch += yOffset;
        m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

        RecalculateVectors();
        m_Position = m_FocalPoint - m_Forward * m_Distance;
        RecalculateViewMatrix();
    }

    void PerspectiveCamera::Zoom(float delta) {
        float speed = std::clamp(m_MoveSpeed * 0.45f, 0.8f, 15.0f);
        m_Position += m_Forward * (delta * speed);
        m_FocalPoint = m_Position + m_Forward * m_Distance;
        RecalculateViewMatrix();
    }

    void PerspectiveCamera::Focus(const glm::vec3& target, float distance) {
        m_FocalPoint = target;
        m_Distance = std::max(1.0f, distance);
        m_Position = target - m_Forward * m_Distance;
        RecalculateViewMatrix();
    }

}
