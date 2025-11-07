#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "model.h"
#include <OpenGL/gl3.h>
#include <stdexcept>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 tex;
};

Model::Model(const string& objPath) {
    tinyobj::ObjReaderConfig config;
    config.mtl_search_path = fs::path(objPath).parent_path().string();
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(objPath, config)) {
        throw runtime_error("Failed to load OBJ: " + objPath + " " + reader.Error());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    unordered_map<int, unsigned> matTex;
    for (size_t i = 0; i < materials.size(); i++) {
        const auto& mat = materials[i];
        if (!mat.diffuse_texname.empty()) {
            fs::path texPath = fs::path(config.mtl_search_path) / mat.diffuse_texname;
            matTex[(int)i] = texCache_.getOrLoad2D(texPath.string());
        }
    }

    // 對每個 shape 產生 mesh
    for (const auto& shape : shapes) {
        vector<Vertex> vertices;
        vector<unsigned> indices;

        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];
            int matID = shape.mesh.material_ids[f];

            for (int v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                Vertex vert{};

                vert.pos = glm::vec3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                );

                if (idx.normal_index >= 0) {
                    vert.normal = glm::vec3(
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    );
                }

                if (idx.texcoord_index >= 0) {
                    vert.tex = glm::vec2(
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]
                    );
                }

                vertices.push_back(vert);
                indices.push_back((unsigned)indices.size());
            }
            indexOffset += fv;

            // 當下一個面材質不同或結束時，建立一個新 Mesh
            bool boundary = (f + 1 == shape.mesh.num_face_vertices.size()) ||
                            (shape.mesh.material_ids[f + 1] != matID);
            if (boundary && !indices.empty()) {
                Mesh mesh;
                glGenVertexArrays(1, &mesh.vao);
                glBindVertexArray(mesh.vao);

                glGenBuffers(1, &mesh.vbo);
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

                glGenBuffers(1, &mesh.ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(glm::vec3)));

                mesh.indexCount = (unsigned)indices.size();
                mesh.textureID = matTex.count(matID) ? matTex[matID] : 0;
                meshes_.push_back(mesh);

                vertices.clear();
                indices.clear();
            }
        }
    }
}

Model::~Model() {
    for (auto& m : meshes_) {
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
    }
}

void Model::Draw() const {
    static unsigned defaultTex = 0;

    // 若沒有建立 default texture，建一張灰階 1x1
    if (defaultTex == 0) {
        unsigned char gray[3] = {128, 128, 128};
        glGenTextures(1, &defaultTex);
        glBindTexture(GL_TEXTURE_2D, defaultTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    for (const auto& mesh : meshes_) {
        glActiveTexture(GL_TEXTURE0);
        unsigned tex = mesh.textureID ? mesh.textureID : defaultTex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
}

