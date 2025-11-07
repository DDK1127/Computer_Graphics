#pragma once
#include <string>
#include <glm/glm.hpp>

// GLSL 編譯與 uniform 設定管理
class Shader
{
public:
    unsigned int id = 0;

    Shader(const char *vertexPath, const char *fragmentPath);
    ~Shader();

    void use() const;
    void setMat4(const char *name, const glm::mat4 &value) const;
    void setVec3(const char *name, const glm::vec3 &value) const;
    void setInt(const char *name, int value) const;
    void setFloat(const char *name, float value) const;
};
