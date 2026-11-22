#include "ShadowMap.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>

namespace Overflow {

    ShadowMap::ShadowMap(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height) {
        Init();
    }

    ShadowMap::~ShadowMap() {
        glDeleteFramebuffers(1, &m_RendererID);
        glDeleteTextures(1, &m_DepthTexture);
    }

    void ShadowMap::Init() {
        glGenFramebuffers(1, &m_RendererID);

        glGenTextures(1, &m_DepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_Width, m_Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthTexture, 0);

        // Desabilita buffers de cor (apenas profundidade importa para sombra)
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            OVERFLOW_CORE_ERROR("Erro: ShadowMap Framebuffer incompleto!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ShadowMap::Bind() {
        glViewport(0, 0, m_Width, m_Height);
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void ShadowMap::Unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ShadowMap::BindTexture(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
    }

    void ShadowMap::CalculateLightSpaceMatrix(
        const glm::vec3& lightDir,
        const glm::vec3& sceneCenter,
        float orthoSize,
        float nearPlane,
        float farPlane
    ) {
        glm::vec3 normLightDir = glm::normalize(lightDir);
        glm::vec3 lightPos = sceneCenter + normLightDir * 20.0f;

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(normLightDir, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
        glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

        m_LightSpaceMatrix = lightProjection * lightView;
    }

}
