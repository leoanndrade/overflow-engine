#include "OpenGLShader.h"
#include "Engine/Core/Log.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <filesystem>
#include <vector>

namespace Overflow {

    static GLenum ShaderTypeFromString(const std::string& type) {
        if (type == "vertex")
            return GL_VERTEX_SHADER;
        if (type == "fragment" || type == "pixel")
            return GL_FRAGMENT_SHADER;
        if (type == "geometry")
            return GL_GEOMETRY_SHADER;

        OVERFLOW_CORE_ERROR("Tipo de shader desconhecido: '{0}'!", type);
        return 0;
    }

    OpenGLShader::OpenGLShader(const std::string& filepath)
        : m_FilePath(filepath) {
        std::string source = ReadFile(filepath);
        auto shaderSources = PreProcess(source);
        Compile(shaderSources);

        // Extrai o nome a partir do nome do arquivo (ex: "assets/shaders/FlatColor.glsl" -> "FlatColor")
        std::filesystem::path path = filepath;
        m_Name = path.stem().string();
    }

    OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_Name(name) {
        std::unordered_map<GLenum, std::string> sources;
        sources[GL_VERTEX_SHADER] = vertexSrc;
        sources[GL_FRAGMENT_SHADER] = fragmentSrc;
        Compile(sources);
    }

    OpenGLShader::~OpenGLShader() {
        glDeleteProgram(m_RendererID);
    }

    std::string OpenGLShader::ReadFile(const std::string& filepath) {
        std::string result;
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            size_t size = static_cast<size_t>(in.tellg());
            if (size != static_cast<size_t>(-1)) {
                result.resize(size);
                in.seekg(0, std::ios::beg);
                in.read(&result[0], size);
            } else {
                OVERFLOW_CORE_ERROR("Nao foi possivel ler o arquivo de shader: '{0}'", filepath);
            }
        } else {
            OVERFLOW_CORE_ERROR("Nao foi possivel abrir o arquivo de shader: '{0}'", filepath);
        }
        return result;
    }

    std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source) {
        std::unordered_map<GLenum, std::string> shaderSources;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);

        while (pos != std::string::npos) {
            size_t eol = source.find_first_of("\r\n", pos);
            size_t begin = pos + typeTokenLength + 1;
            std::string type = source.substr(begin, eol - begin);

            // Limpeza de espaços em branco
            type.erase(0, type.find_first_not_of(" \t\r\n"));
            type.erase(type.find_last_not_of(" \t\r\n") + 1);

            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            pos = source.find(typeToken, nextLinePos);

            shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos)
                ? source.substr(nextLinePos)
                : source.substr(nextLinePos, pos - nextLinePos);
        }

        return shaderSources;
    }

    void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources) {
        GLuint program = glCreateProgram();
        std::vector<GLenum> glShaderIDs;
        glShaderIDs.reserve(shaderSources.size());

        for (auto& kv : shaderSources) {
            GLenum type = kv.first;
            const std::string& source = kv.second;

            GLuint shader = glCreateShader(type);
            const GLchar* sourceCStr = source.c_str();
            glShaderSource(shader, 1, &sourceCStr, 0);
            glCompileShader(shader);

            GLint isCompiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE) {
                GLint maxLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

                std::vector<GLchar> infoLog(maxLength);
                glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

                glDeleteShader(shader);

                OVERFLOW_CORE_ERROR("Falha na compilacao do Shader: {0}", infoLog.data());
                break;
            }

            glAttachShader(program, shader);
            glShaderIDs.push_back(shader);
        }

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(program);

            for (auto id : glShaderIDs) {
                glDeleteShader(id);
            }

            OVERFLOW_CORE_ERROR("Falha no link do Programa de Shader: {0}", infoLog.data());
            return;
        }

        for (auto id : glShaderIDs) {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        m_RendererID = program;
        OVERFLOW_CORE_INFO("Shader compilado e linkado com sucesso: '{0}' (ID: {1})", m_Name, m_RendererID);
    }

    void OpenGLShader::Bind() const {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const {
        glUseProgram(0);
    }

    int OpenGLShader::GetUniformLocation(const std::string& name) {
        if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
            return m_UniformLocationCache[name];

        int location = glGetUniformLocation(m_RendererID, name.c_str());
        if (location == -1) {
            OVERFLOW_CORE_WARN("Aviso: Uniform '{0}' nao encontrado no shader '{1}'!", name, m_Name);
        }

        m_UniformLocationCache[name] = location;
        return location;
    }

    void OpenGLShader::SetInt(const std::string& name, int value) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform1i(location, value);
    }

    void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform1iv(location, count, values);
    }

    void OpenGLShader::SetFloat(const std::string& name, float value) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform1f(location, value);
    }

    void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform2f(location, value.x, value.y);
    }

    void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform3f(location, value.x, value.y, value.z);
    }

    void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::SetMat3(const std::string& name, const glm::mat3& matrix) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& matrix) {
        int location = GetUniformLocation(name);
        if (location != -1) glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

}
