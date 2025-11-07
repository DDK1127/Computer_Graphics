#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position {0.0f, 5.0f, 20.0f}; // 攝影機位置
    glm::vec3 target   {0.0f, 5.0f, 0.0f};  // 注視點
    glm::vec3 up       {0.0f, 1.0f, 0.0f};  // 向上方向

    // 直接設定位置與注視點
    void setPosition(const glm::vec3& pos)  { position = pos; }
    void setTarget(const glm::vec3& lookAt) { target = lookAt; }

    // 取得 view matrix
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, up);
    }
};
