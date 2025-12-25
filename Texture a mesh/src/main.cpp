#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <vector>
#include <string>
#include <cfloat>
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>
namespace fs = std::filesystem;

// ======================================================
// 工具函式
// ======================================================

static void glfw_err(int code, const char *desc) { std::fprintf(stderr, "GLFW %d: %s\n", code, desc); }

static void normalize_center(std::vector<float> &pos)
{
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    for (size_t i = 0; i < pos.size(); i += 3)
    {
        glm::vec3 p(pos[i], pos[i + 1], pos[i + 2]);
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    glm::vec3 size = mx - mn;
    float maxDim = std::max(size.x, std::max(size.y, size.z));
    if (maxDim <= 0)
        return;
    float s = 1.0f / maxDim;
    glm::vec3 c = 0.5f * (mn + mx);
    for (size_t i = 0; i < pos.size(); i += 3)
    {
        glm::vec3 p(pos[i], pos[i + 1], pos[i + 2]);
        p = (p - c) * s;
        pos[i] = p.x;
        pos[i + 1] = p.y;
        pos[i + 2] = p.z;
    }
}

struct GLTexture
{
    GLuint id = 0;
    int w = 0, h = 0;
};

GLTexture loadTexture2D(const std::string &path)
{
    stbi_set_flip_vertically_on_load(true);
    int w, h, n;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &n, 0);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << "\n";
        return {};
    }
    GLenum fmt = n == 1 ? GL_RED : n == 3 ? GL_RGB
                                          : GL_RGBA;
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    return {id, w, h};
}

static GLuint makeProgram(const char *vsPath, const char *fsPath)
{
    auto read = [](const char *p)
    {
        FILE *f = fopen(p, "rb");
        if (!f)
            return std::string();
        fseek(f, 0, SEEK_END);
        long L = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string s;
        s.resize(L);
        fread(s.data(), 1, L, f);
        fclose(f);
        return s;
    };
    auto compile = [](GLenum type, const std::string &src)
    {
        GLuint s = glCreateShader(type);
        const char *c = src.c_str();
        glShaderSource(s, 1, &c, nullptr);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            GLint len;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(s, len, nullptr, log.data());
            std::cerr << log << "\n";
        }
        return s;
    };
    std::string vs = read(vsPath), fs = read(fsPath);
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << log << "\n";
    }
    return prog;
}

// ======================================================
// 主程式
// ======================================================

struct DrawCall
{
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    int materialId = -1;
};

int main()
{

    glfwSetErrorCallback(glfw_err);
    if (!glfwInit())
        return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow *win = glfwCreateWindow(1280, 720, "HW2 Textured Mesh", nullptr, nullptr);
    if (!win)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "glad load failed\n";
        return -1;
    }
    // === 自動找執行檔所在目錄 ===
    fs::path exe_dir = fs::current_path();
    fs::path asset_dir = exe_dir / "assets";
    fs::path shader_dir = exe_dir / "shaders";

    GLuint prog = makeProgram("shaders/mesh.vert", "shaders/mesh.frag");

    // ------------------------------------------------------
    // 讀取模型
    // ------------------------------------------------------
    std::string base = "assets/";
    std::string inputfile = (asset_dir / MODEL_FILE).string();

    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    cfg.vertex_color = false;
    cfg.mtl_search_path = base;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(inputfile, cfg))
    {
        std::cerr << reader.Error();
        return -1;
    }
    if (!reader.Warning().empty())
        std::cerr << reader.Warning();

    // 若沒有法線，自己生成
    auto &attrib = const_cast<tinyobj::attrib_t &>(reader.GetAttrib());
    // 強制重新生成法線
    attrib.normals.clear();
    {
        std::vector<glm::vec3> computed_normals(attrib.vertices.size() / 3, glm::vec3(0));
        for (const auto &shape : reader.GetShapes())
        {
            const auto &indices = shape.mesh.indices;
            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                int i0 = indices[i + 0].vertex_index;
                int i1 = indices[i + 1].vertex_index;
                int i2 = indices[i + 2].vertex_index;
                glm::vec3 p0(attrib.vertices[3 * i0 + 0], attrib.vertices[3 * i0 + 1], attrib.vertices[3 * i0 + 2]);
                glm::vec3 p1(attrib.vertices[3 * i1 + 0], attrib.vertices[3 * i1 + 1], attrib.vertices[3 * i1 + 2]);
                glm::vec3 p2(attrib.vertices[3 * i2 + 0], attrib.vertices[3 * i2 + 1], attrib.vertices[3 * i2 + 2]);
                glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                computed_normals[i0] += n;
                computed_normals[i1] += n;
                computed_normals[i2] += n;
            }
        }
        attrib.normals.resize(computed_normals.size() * 3);
        for (size_t i = 0; i < computed_normals.size(); ++i)
        {
            glm::vec3 n = glm::normalize(computed_normals[i]);
            attrib.normals[3 * i + 0] = n.x;
            attrib.normals[3 * i + 1] = n.y;
            attrib.normals[3 * i + 2] = n.z;
        }
    }

    const auto &shapes = reader.GetShapes();
    const auto &materials = reader.GetMaterials();

    // 載入貼圖
    std::vector<GLTexture> textures(materials.size());
    for (size_t i = 0; i < materials.size(); ++i)
    {
        std::string texname = materials[i].diffuse_texname.empty() ? "tiger-atlas.jpg" : materials[i].diffuse_texname;
        textures[i] = loadTexture2D(base + texname);
    }
    if (textures.empty())
        textures.push_back(loadTexture2D(base + "tiger-atlas.jpg"));

    // ------------------------------------------------------
    // 建立所有 shape 的 VBO/EBO
    // ------------------------------------------------------
    std::vector<float> all_positions;
    std::vector<DrawCall> draws;
    for (const auto &sh : shapes)
    {
        std::vector<float> verts;
        std::vector<unsigned int> idx;
        for (const auto &index : sh.mesh.indices)
        {
            glm::vec3 p(0);
            if (index.vertex_index >= 0)
            {
                p.x = attrib.vertices[3 * index.vertex_index + 0];
                p.y = attrib.vertices[3 * index.vertex_index + 1];
                p.z = attrib.vertices[3 * index.vertex_index + 2];
            }
            glm::vec3 n(0);
            if (index.normal_index >= 0)
            {
                n.x = attrib.normals[3 * index.normal_index + 0];
                n.y = attrib.normals[3 * index.normal_index + 1];
                n.z = attrib.normals[3 * index.normal_index + 2];
            }
            glm::vec2 uv(0);
            if (index.texcoord_index >= 0)
            {
                uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                uv.y = attrib.texcoords[2 * index.texcoord_index + 1];
            }
            verts.insert(verts.end(), {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
            idx.push_back(idx.size());
            all_positions.push_back(p.x);
            all_positions.push_back(p.y);
            all_positions.push_back(p.z);
        }

        DrawCall d;
        glGenVertexArrays(1, &d.vao);
        glBindVertexArray(d.vao);
        glGenBuffers(1, &d.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &d.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)(sizeof(float) * 3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)(sizeof(float) * 6));
        d.indexCount = (GLsizei)idx.size();
        d.materialId = sh.mesh.material_ids.empty() ? -1 : sh.mesh.material_ids[0];
        draws.push_back(d);
    }

    // 整體 normalize 一次
    normalize_center(all_positions);

    // 把新位置寫回各 VBO
    size_t pos_i = 0;
    for (const auto &d : draws)
    {
        glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
        float *buf = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (buf)
        {
            for (int i = 0; i < d.indexCount; i++)
            {
                buf[i * 8 + 0] = all_positions[pos_i++];
                buf[i * 8 + 1] = all_positions[pos_i++];
                buf[i * 8 + 2] = all_positions[pos_i++];
            }
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
    }

    // ------------------------------------------------------
    // 渲染
    // ------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
    OrbitCamera cam;
    double lastx = 0, lasty = 0;
    bool dragging = false;

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();
        // 旋轉控制
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double x, y;
            glfwGetCursorPos(win, &x, &y);
            if (!dragging)
            {
                dragging = true;
                lastx = x;
                lasty = y;
            }
            else
            {
                cam.rotate(float(x - lastx), float(y - lasty));
                lastx = x;
                lasty = y;
            }
        }
        else
            dragging = false;

        // 滾輪縮放
        double scrollY = 0.0;
        glfwSetScrollCallback(win, [](GLFWwindow *w, double xoff, double yoff)
                              {
        OrbitCamera* cam = (OrbitCamera*)glfwGetWindowUserPointer(w);
        cam->zoom((float)yoff); });
        glfwSetWindowUserPointer(win, &cam);

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.15f, 0.18f, 0.22f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // === 新增旋轉控制 ===
        static float modelYaw = 0.0f;
        static bool draggingModel = false;
        static double lastx = 0.0;

        // 左右拖曳旋轉模型
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double x, y;
            glfwGetCursorPos(win, &x, &y);
            if (!draggingModel)
            {
                draggingModel = true;
                lastx = x;
            }
            else
            {
                float dx = float(x - lastx);
                modelYaw += dx * 0.3f; // 旋轉速度係數
                lastx = x;
            }
        }
        else
            draggingModel = false;

        // 組合旋轉矩陣（繞 Y 軸）
        glm::mat4 model = glm::rotate(glm::mat4(1.0f),
                                      glm::radians(modelYaw),
                                      glm::vec3(0, 1, 0));
        glm::mat4 V = cam.view();
        glm::mat4 P = glm::perspective(glm::radians(50.0f),
                                       float(w) / float(h),
                                       0.01f, 50.0f);

        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, &model[0][0]);

        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, &V[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, &P[0][0]);

        glm::vec3 camPos = glm::vec3(glm::inverse(V)[3]);
        glUniform3fv(glGetUniformLocation(prog, "uCam"), 1, &camPos[0]);
        glUniform1i(glGetUniformLocation(prog, "uTex"), 0);

        for (const auto &d : draws)
        {
            int mid = d.materialId >= 0 ? d.materialId : 0;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[mid].id);
            glBindVertexArray(d.vao);
            glDrawElements(GL_TRIANGLES, d.indexCount, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(win);
    }

    glfwTerminate();
    return 0;
}
