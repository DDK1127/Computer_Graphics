#pragma once
#include <string>
#include <unordered_map>

// 管理貼圖載入與快取
class TextureCache {
public:
    unsigned getOrLoad2D(const std::string& path);
    void clear();
private:
    std::unordered_map<std::string, unsigned> cache_;
};
