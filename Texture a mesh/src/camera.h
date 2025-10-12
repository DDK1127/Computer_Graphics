#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct OrbitCamera {
  float dist = 2.5f;   // 與物體距離
  float yaw = 0.0f;    // 左右角度
  float pitch = 0.3f;  // 上下角度
  glm::vec3 target = glm::vec3(0);

  glm::mat4 view() const {
    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);
    glm::vec3 eye = target + dist * glm::vec3(cp*cy, sp, cp*sy);
    return glm::lookAt(eye, target, glm::vec3(0,1,0));
  }

  // 用滑鼠輸入更新相機
  void rotate(float dx, float dy) {
    const float sensitivity = 0.003f;
    yaw   += dx * sensitivity;
    pitch += dy * sensitivity;
    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;
  }

  void zoom(float delta) {
    dist *= (1.0f - delta * 0.1f);
    if (dist < 0.3f) dist = 0.3f;
    if (dist > 20.0f) dist = 20.0f;
  }
};
