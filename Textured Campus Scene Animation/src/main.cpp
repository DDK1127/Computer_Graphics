#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
#include <vector>
#include <cmath>

#include "shader.h"
#include "camera.h"
#include "model.h"

// -----------------------------------------------------------------------------
// Catmull–Rom 插值
// -----------------------------------------------------------------------------
static glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1,
                            const glm::vec3& p2, const glm::vec3& p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.f * p1) +
                   (-p0 + p2) * t +
                   (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                   (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
}

// -----------------------------------------------------------------------------
// GLFW 錯誤輸出
// -----------------------------------------------------------------------------
static void glfwErrorCallback(int code, const char* desc)
{
    std::cerr << "GLFW Error " << code << ": " << desc << std::endl;
}

// -----------------------------------------------------------------------------
// 主程式
// -----------------------------------------------------------------------------
int main()
{
    // --- 確保相對路徑正確 ---
    std::filesystem::path execDir = std::filesystem::current_path();
    if (execDir.filename() == "build")
        std::filesystem::current_path(execDir.parent_path());

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Campus Cinematic Stable View", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glEnable(GL_DEPTH_TEST);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
    });

    // --- Shader & Model ---
    Shader shader("shaders/vertex_shader.vs", "shaders/fragment_shader.fs");
    shader.use();
    shader.setInt("uDiffuse", 0);

    Model campus("assets/SchoolSceneDay/SchoolSceneDay.obj");
    Camera camera;

    // -------------------------------------------------------------------------
    // 鏡頭段落設計
    // -------------------------------------------------------------------------
    struct PathSegment {
        std::vector<glm::vec3> pts;
        float duration;
        glm::vec3 lookCenter;
    };

    std::vector<PathSegment> segments = {
        {
            {
                { -100, 100, -56},
                {  -60, 85,  -56},
                {  -20, 70,  -55},
                {   40, 15,  -55},
                { 31, 1.5,   -31},
                { 31, 1.6,   11},
                { 31, 1.5,   27},
                { 31, 1.5,   31},
                { 0, 1.5,   31},
                { -21.5, 1.5,  30},
                { -21.5, 1.5,  17},
                { -15, 4,  10},
                { -15, 4,  10}
            },
            35.0f,
            glm::vec3(-13, -5, 61)
        },

        {
            {
                { -21, 30,  78},
                { -21, 20,  78},
                {  79, 17,  49},
                {  40, 15,   -49},
                {  -46, 16,  4},
                {  -47, 20,  84},
                {  -47, 30,  -84}
            },
            20.0f,
            glm::vec3(46, 30, 72)
        },
    };

    float totalDuration = 0.f;
    for (auto& s : segments) totalDuration += s.duration;
    double startTime = glfwGetTime();

    // -------------------------------------------------------------------------
    // 主迴圈
    // -------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime() - startTime;
        float tGlobal = fmodf((float)now, totalDuration);

        // 決定目前段落
        float acc = 0.f;
        int segIdx = 0;
        for (int i = 0; i < (int)segments.size(); ++i) {
            if (tGlobal < acc + segments[i].duration) { segIdx = i; break; }
            acc += segments[i].duration;
        }
        float localT = (tGlobal - acc) / segments[segIdx].duration;

        // 插值
        auto& path = segments[segIdx].pts;
        int n = path.size();
        float segment = localT * (n - 3);
        int i = (int)floorf(segment);
        float t = segment - i;
        glm::vec3 camPos = catmullRom(path[i + 0], path[i + 1], path[i + 2], path[i + 3], t);

        // 高度下限保護
        camPos.y = std::max(camPos.y, 0.0f);

        // 注視點
        glm::vec3 nextPos = catmullRom(path[i], path[i+1], path[i+2], path[i+3], t + 0.02f);
        glm::vec3 desiredTarget = nextPos; // 看向移動方向


        // 設定相機位置與注視點（直接設定）
        camera.setPosition(camPos);
        camera.setTarget(desiredTarget);

        // 光照方向（太陽微移）
        float sunAngle = glm::radians((float)now * 5.0f);
        glm::vec3 sunDir = glm::normalize(glm::vec3(sin(sunAngle), -1.0f, cos(sunAngle)));

        // Retina framebuffer 尺寸
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);

        // 清除畫面
        glClearColor(0.7f, 0.85f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 矩陣
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)fbW / fbH, 1.0f, 500.0f);
        glm::mat4 model = glm::mat4(1.0f);

        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", proj);
        shader.setMat4("model", model);
        shader.setVec3("lightDir", sunDir);
        glfwSwapInterval(1);

        campus.Draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
} 