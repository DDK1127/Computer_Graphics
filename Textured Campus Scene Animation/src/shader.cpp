#include "shader.h"
#include <OpenGL/gl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

static std::string readFile(const char *path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error(std::string("Cannot open ") + path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static unsigned int compileStage(GLenum type, const char *src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compile error:\n"
                  << log << std::endl;
        throw std::runtime_error("Shader compilation failed.");
    }
    return shader;
}

Shader::Shader(const char *vertexPath, const char *fragmentPath)
{
    std::string vSrc = readFile(vertexPath);
    std::string fSrc = readFile(fragmentPath);

    unsigned int vs = compileStage(GL_VERTEX_SHADER, vSrc.c_str());
    unsigned int fs = compileStage(GL_FRAGMENT_SHADER, fSrc.c_str());

    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);

    int success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(id, 1024, nullptr, log);
        std::cerr << "Program link error:\n"
                  << log << std::endl;
        throw std::runtime_error("Shader program link failed.");
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader()
{
    if (id)
        glDeleteProgram(id);
}

void Shader::use() const
{
    glUseProgram(id);
}

void Shader::setMat4(const char *name, const glm::mat4 &value) const
{
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const char *name, const glm::vec3 &value) const
{
    glUniform3fv(glGetUniformLocation(id, name), 1, &value[0]);
}

void Shader::setInt(const char *name, int value) const
{
    glUniform1i(glGetUniformLocation(id, name), value);
}

void Shader::setFloat(const char *name, float value) const
{
    glUniform1f(glGetUniformLocation(id, name), value);
}
