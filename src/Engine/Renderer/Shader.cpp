#include "Shader.h"
#include "Engine/Platform/OpenGL/OpenGLShader.h"
#include "Engine/Core/Log.h"

namespace Overflow {

    std::shared_ptr<Shader> Shader::Create(const std::string& filepath) {
        return std::make_shared<OpenGLShader>(filepath);
    }

    std::shared_ptr<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
        return std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);
    }

    void ShaderLibrary::Add(const std::string& name, const std::shared_ptr<Shader>& shader) {
        if (Exists(name)) {
            OVERFLOW_CORE_WARN("Shader '{0}' ja existe na ShaderLibrary!", name);
            return;
        }
        m_Shaders[name] = shader;
    }

    void ShaderLibrary::Add(const std::shared_ptr<Shader>& shader) {
        auto& name = shader->GetName();
        Add(name, shader);
    }

    std::shared_ptr<Shader> ShaderLibrary::Load(const std::string& filepath) {
        auto shader = Shader::Create(filepath);
        Add(shader);
        return shader;
    }

    std::shared_ptr<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath) {
        auto shader = Shader::Create(filepath);
        Add(name, shader);
        return shader;
    }

    std::shared_ptr<Shader> ShaderLibrary::Get(const std::string& name) {
        if (!Exists(name)) {
            OVERFLOW_CORE_ERROR("Shader '{0}' nao encontrado na ShaderLibrary!", name);
            return nullptr;
        }
        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const std::string& name) const {
        return m_Shaders.find(name) != m_Shaders.end();
    }

}
