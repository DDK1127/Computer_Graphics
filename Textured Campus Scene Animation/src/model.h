#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "texture_cache.h"

// 單一 Mesh 結構
struct Mesh {
    unsigned vao = 0, vbo = 0, ebo = 0;
    unsigned indexCount = 0;
    unsigned textureID = 0;
};

// 模型載入與繪製
class Model {
public:
    explicit Model(const std::string& objPath);
    ~Model();

    void Draw() const;

private:
    std::vector<Mesh> meshes_;
    TextureCache texCache_;
};
