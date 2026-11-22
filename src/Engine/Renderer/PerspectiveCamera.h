#pragma once

#include "Camera.h"
#include "Engine/Core/Timestep.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Overflow {

    class PerspectiveCamera : public Camera {
    public:
        PerspectiveCamera(float fov = 45.0f, float aspectRatio = 1.778f, float nearClip = 0.1f, float farClip = 1000.0f);

        void OnUpdate(Timestep ts);
        void SetViewportSize(uint32_t width, uint32_t height);

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        void SetRotation(float pitch, float yaw) { m_Pitch = pitch; m_Yaw = yaw; RecalculateVectors(); }

        const glm::vec3& GetForward() const { return m_Forward; }
        const glm::vec3& GetRight() const { return m_Right; }
        const glm::vec3& GetUp() const { return m_Up; }

        float GetMoveSpeed() const { return m_MoveSpeed; }
        void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

        void Focus(const glm::vec3& target, float distance = 5.0f);

        const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }
        void SetFocalPoint(const glm::vec3& focalPoint) { m_FocalPoint = focalPoint; }

        float GetDistance() const { return m_Distance; }
        void SetDistance(float distance) { m_Distance = distance; }

        void Pan(const glm::vec2& delta);
        void Orbit(const glm::vec2& delta);
        void Zoom(float delta);

        float GetNearClip() const { return m_NearClip; }
        float GetFarClip() const { return m_FarClip; }
        float GetFOV() const { return m_FOV; }
        float GetAspectRatio() const { return m_AspectRatio; }

        float GetMouseSensitivity() const { return m_MouseSensitivity; }
        void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
        void ResetMouseState() { m_FirstMouse = true; }

    private:
        void RecalculateProjection();
        void RecalculateVectors();
        void RecalculateViewMatrix();

    private:
        float m_FOV;
        float m_AspectRatio;
        float m_NearClip;
        float m_FarClip;

        glm::vec3 m_Position = { 0.0f, 0.0f, 5.0f };
        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
        float m_Distance = 5.0f;
        glm::vec3 m_Forward = { 0.0f, 0.0f, -1.0f };
        glm::vec3 m_Up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 m_Right = { 1.0f, 0.0f, 0.0f };
        glm::vec3 m_WorldUp = { 0.0f, 1.0f, 0.0f };

        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;

        float m_MoveSpeed = 5.0f;
        float m_MouseSensitivity = 0.15f;

        glm::vec2 m_LastMousePosition = { 0.0f, 0.0f };
        bool m_FirstMouse = true;

        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
    };

}
