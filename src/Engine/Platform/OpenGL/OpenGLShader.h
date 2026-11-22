#pragma once

#include "Engine/Renderer/Shader.h"
#include <unordered_map>

namespace Overflow {

    class OpenGLShader : public Shader {
    public:
        OpenGLShader(const std::string& filepath);
        OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        virtual ~OpenGLShader() override;

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual const std::string& GetName() const override { return m_Name; }

        virtual void SetInt(const std::string& name, int value) override;
        virtual void SetIntArray(const std::string& name, int* values, uint32_t count) override;
        virtual void SetFloat(const std::string& name, float value) override;
        virtual void SetFloat2(const std::string& name, const glm::vec2& value) override;
        virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
        virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;
        virtual void SetMat3(const std::string& name, const glm::mat3& matrix) override;
        virtual void SetMat4(const std::string& name, const glm::mat4& matrix) override;

    private:
        std::string ReadFile(const std::string& filepath);
        std::unordered_map<unsigned int, std::string> PreProcess(const std::string& source);
        void Compile(const std::unordered_map<unsigned int, std::string>& shaderSources);
        int GetUniformLocation(const std::string& name);

    private:
        uint32_t m_RendererID = 0;
        std::string m_FilePath;
        std::string m_Name;
        mutable std::unordered_map<std::string, int> m_UniformLocationCache;
    };

}
