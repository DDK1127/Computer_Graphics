#include "texture_cache.h"
#include <OpenGL/gl3.h>
#include <stdexcept>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

unsigned TextureCache::getOrLoad2D(const std::string &path)
{
    auto it = cache_.find(path);
    if (it != cache_.end())
        return it->second;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // OpenGL Y 軸反轉
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data)
        throw std::runtime_error("Failed to load texture: " + path);

    GLenum format = GL_RGB;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;

    unsigned tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Filter / wrap 設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    cache_[path] = tex;

    std::cout << "Loaded texture: " << path << std::endl;
    return tex;
}

void TextureCache::clear()
{
    for (auto &[path, id] : cache_)
    {
        glDeleteTextures(1, &id);
    }
    cache_.clear();
}
