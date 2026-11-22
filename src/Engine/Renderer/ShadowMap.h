#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>

namespace Overflow {

    class ShadowMap {
    public:
        ShadowMap(uint32_t width = 2048, uint32_t height = 2048);
        ~ShadowMap();

        void Bind();
        void Unbind();

        void BindTexture(uint32_t slot = 1) const;

        void CalculateLightSpaceMatrix(
            const glm::vec3& lightDir,
            const glm::vec3& sceneCenter = { 0.0f, 0.0f, 0.0f },
            float orthoSize = 15.0f,
            float nearPlane = 0.1f,
            float farPlane = 50.0f
        );

        const glm::mat4& GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }
        uint32_t GetDepthTextureID() const { return m_DepthTexture; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        void Init();

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_DepthTexture = 0;
        uint32_t m_Width;
        uint32_t m_Height;
        glm::mat4 m_LightSpaceMatrix = glm::mat4(1.0f);
    };

}
