// main.cpp — OBJ loader + unit normalize + center + basic lighting (OpenGL 3.3)
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <vector>
#include <string>
#include <cfloat>
#include <cmath>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

static const char* VS_SRC = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
uniform mat4 uModel,uView,uProj;
out vec3 vN; out vec3 vWPos;
void main(){
  vec4 wpos = uModel * vec4(aPos,1.0);
  vWPos = wpos.xyz;
  vN = mat3(transpose(inverse(uModel))) * aNrm;
  gl_Position = uProj * uView * wpos;
})";

static const char* FS_SRC = R"(#version 330 core
in vec3 vN; in vec3 vWPos;
uniform vec3 uCamPos;
out vec4 FragColor;
void main(){
  vec3 N = normalize(vN);
  vec3 L = normalize(vec3(0.7,1.0,0.5));
  vec3 V = normalize(uCamPos - vWPos);
  vec3 H = normalize(L+V);
  float diff = max(dot(N,L),0.0);
  float spec = pow(max(dot(N,H),0.0), 32.0);
  vec3 base = vec3(0.75,0.80,1.0);
  vec3 col = 0.08*base + 0.85*diff*base + 0.35*spec;
  FragColor = vec4(col,1.0);
})";

// 背景用的 Vertex Shader
static const char* BG_VS = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 uv;
void main(){
    uv = aPos * 0.5 + 0.5;  // 把 -1~1 映射到 0~1
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

// 背景用的 Fragment Shader
static const char* BG_FS = R"(#version 330 core
in vec2 uv;
out vec4 FragColor;
void main(){
    vec3 topColor    = vec3(0.5, 0.75, 1.0); // 天藍
    vec3 bottomColor = vec3(1.0, 0.85, 0.6); // 淡橘（地平線）
    vec3 col = mix(bottomColor, topColor, uv.y);
    FragColor = vec4(col, 1.0);
})";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetShaderInfoLog(s, len, nullptr, &log[0]);
        std::cerr << "Shader error:\n" << log << "\n"; exit(1);
    }
    return s;
}
static GLuint link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetProgramInfoLog(p, len, nullptr, &log[0]);
        std::cerr << "Link error:\n" << log << "\n"; exit(1);
    }
    glDeleteShader(vs); glDeleteShader(fs); return p;
}

static void normalize_center(std::vector<float>& pos) {
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    for (size_t i = 0; i < pos.size(); i += 3) {
        glm::vec3 p(pos[i], pos[i + 1], pos[i + 2]); mn = glm::min(mn, p); mx = glm::max(mx, p);
    }
    glm::vec3 size = mx - mn; float maxDim = std::max(size.x, std::max(size.y, size.z));
    if (maxDim <= 0) return; float s = 1.0f / maxDim; glm::vec3 c = 0.5f * (mn + mx);
    for (size_t i = 0; i < pos.size(); i += 3) {
        glm::vec3 p(pos[i], pos[i + 1], pos[i + 2]); p = (p - c) * s;
        pos[i] = p.x; pos[i + 1] = p.y; pos[i + 2] = p.z;
    }
}
static void compute_normals(const std::vector<float>& pos, std::vector<float>& nrm) {
    bool has = false; for (float v : nrm) { if (v != 0) { has = true; break; } }
    if (has) return;
    nrm.assign(pos.size(), 0.0f);
    for (size_t i = 0; i < pos.size(); i += 9) {
        glm::vec3 p0(pos[i + 0], pos[i + 1], pos[i + 2]);
        glm::vec3 p1(pos[i + 3], pos[i + 4], pos[i + 5]);
        glm::vec3 p2(pos[i + 6], pos[i + 7], pos[i + 8]);
        glm::vec3 fn = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        for (int k = 0; k < 3; ++k) { nrm[i + 3 * k + 0] += fn.x; nrm[i + 3 * k + 1] += fn.y; nrm[i + 3 * k + 2] += fn.z; }
    }
    for (size_t i = 0; i < nrm.size(); i += 3) {
        glm::vec3 v(nrm[i], nrm[i + 1], nrm[i + 2]); v = glm::normalize(v);
        if (!std::isfinite(v.x)) v = glm::vec3(0, 1, 0);
        nrm[i] = v.x; nrm[i + 1] = v.y; nrm[i + 2] = v.z;
    }
}

struct Mesh { std::vector<float> inter; size_t count = 0; GLuint vao = 0, vbo = 0; };

static bool load_obj(const std::string& path, Mesh& m) {
    tinyobj::ObjReader reader; tinyobj::ObjReaderConfig cfg; cfg.triangulate = true;
    if (!reader.ParseFromFile(path, cfg)) { std::cerr << "OBJ: " << reader.Error() << "\n"; return false; }
    const auto& a = reader.GetAttrib(); const auto& shapes = reader.GetShapes();
    std::vector<float> pos, nrm;
    for (const auto& sh : shapes) {
        size_t off = 0;
        for (size_t f = 0; f < sh.mesh.num_face_vertices.size(); ++f) {
            int fv = sh.mesh.num_face_vertices[f];
            for (int v = 0; v < fv; ++v) {
                auto idx = sh.mesh.indices[off + v];
                pos.push_back(a.vertices[3 * idx.vertex_index + 0]);
                pos.push_back(a.vertices[3 * idx.vertex_index + 1]);
                pos.push_back(a.vertices[3 * idx.vertex_index + 2]);
                if (idx.normal_index >= 0) {
                    nrm.push_back(a.normals[3 * idx.normal_index + 0]);
                    nrm.push_back(a.normals[3 * idx.normal_index + 1]);
                    nrm.push_back(a.normals[3 * idx.normal_index + 2]);
                }
                else { nrm.insert(nrm.end(), { 0,0,0 }); }
            }
            off += fv;
        }
    }
    normalize_center(pos);
    compute_normals(pos, nrm);
    m.count = pos.size() / 3;
    m.inter.resize(m.count * 6);
    for (size_t i = 0, j = 0; i < pos.size(); i += 3, j += 6) {
        m.inter[j + 0] = pos[i + 0]; m.inter[j + 1] = pos[i + 1]; m.inter[j + 2] = pos[i + 2];
        m.inter[j + 3] = nrm[i + 0]; m.inter[j + 4] = nrm[i + 1]; m.inter[j + 5] = nrm[i + 2];
    }
    return true;
}
static void upload(Mesh& m) {
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, m.inter.size() * sizeof(float), m.inter.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

int main(int argc, char** argv) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(960, 640, "OBJ Viewer (unit-normalized)", nullptr, nullptr);
    if (!win) { std::cerr << "GLFW window failed\n"; return 1; }
    glfwMakeContextCurrent(win);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return 1; }
    glfwSwapInterval(1);

    GLuint prog = link(compile(GL_VERTEX_SHADER, VS_SRC), compile(GL_FRAGMENT_SHADER, FS_SRC));
    GLint uM = glGetUniformLocation(prog, "uModel");
    GLint uV = glGetUniformLocation(prog, "uView");
    GLint uP = glGetUniformLocation(prog, "uProj");
    GLint uC = glGetUniformLocation(prog, "uCamPos");

    std::string path = (argc > 1) ? argv[1] : std::string("Dino.obj");
    Mesh mesh;
    if (!load_obj(path, mesh)) { std::cerr << "Failed to load: " << path << "\n"; return 1; }
    upload(mesh);

    // 全螢幕三角形 (覆蓋整個螢幕)
    float bgVertices[] = {
       -1.0f, -1.0f,
        3.0f, -1.0f,
       -1.0f,  3.0f
    };

    GLuint bgVAO, bgVBO;
    glGenVertexArrays(1, &bgVAO);
    glGenBuffers(1, &bgVBO);
    glBindVertexArray(bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bgVertices), bgVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // 編譯背景 shader
    GLuint bgProg = link(compile(GL_VERTEX_SHADER, BG_VS), compile(GL_FRAGMENT_SHADER, BG_FS));


    glEnable(GL_DEPTH_TEST);
    /*glClearColor(0.60f, 0.80f, 1.00f, 1.0f);*/

    float yaw = -0.7f, pitch = -0.3f, dist = 2.2f; bool dragging = false; double lastX = 0, lastY = 0;

    while (!glfwWindowShouldClose(win)) {
        // orbit (LMB) + zoom (Q/E)
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double x, y; glfwGetCursorPos(win, &x, &y);
            if (!dragging) { dragging = true; lastX = x; lastY = y; }
            double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
            yaw -= (float)dx * 0.005f;
            pitch -= (float)dy * 0.005f; pitch = glm::clamp(pitch, -1.3f, 1.3f);
        }
        else dragging = false;
        if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) dist *= 0.98f;
        if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) dist *= 1.02f;

        int w, h; glfwGetFramebufferSize(win, &w, &h);
        float aspect = (h > 0) ? (float)w / h : 1.6f;
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float cx = cosf(yaw) * cosf(pitch), sx = sinf(yaw) * cosf(pitch), sy = sinf(pitch);
        glm::vec3 dir = glm::normalize(glm::vec3(cx, sy, sx));
        glm::vec3 cam = -dir * dist;

        glm::mat4 M(1.0f);
        glm::mat4 V = glm::lookAt(cam, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 P = glm::perspective(glm::radians(50.0f), aspect, 0.01f, 100.0f);

        // 1) 清除顏色 & 深度
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2) 畫背景 (不需要深度測試)
        glDisable(GL_DEPTH_TEST);
        glUseProgram(bgProg);
        glBindVertexArray(bgVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 3) 畫恐龍 (啟用深度測試)
        glEnable(GL_DEPTH_TEST);
        glUseProgram(prog); // 恐龍的 shader program
        glUniformMatrix4fv(uM, 1, GL_FALSE, &M[0][0]);
        glUniformMatrix4fv(uV, 1, GL_FALSE, &V[0][0]);
        glUniformMatrix4fv(uP, 1, GL_FALSE, &P[0][0]);
        glUniform3f(uC, cam.x, cam.y, cam.z);
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh.count);


        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
