#ifndef TERRAIN_H
#define TERRAIN_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>
#include <string>
#include "../lib/FastNoiseLite.h"

class Terrain {
public:
    Terrain(std::size_t size, float scale, float heightScale,
            const std::string& albedoPath, const std::string& normalPath,
            const std::string& roughnessPath, const std::string& aoPath,
            float smoothness);
    ~Terrain();

    void Draw();
    void Cleanup();

private:
    struct Vertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec3 normal;
    };

    void buildMesh();
    void loadTexture(const std::string& path, GLuint& texID);
    void addRivers(std::vector<Vertex>& verts);

    std::size_t size_;      // number of quads per side
    float scale_;           // world-space width/depth
    float heightScale_;     // vertical exaggeration
    float smoothness_;      // fractal exponent

    FastNoiseLite detailNoise_;
    FastNoiseLite riverNoise_;

    GLuint vao_, vbo_, ebo_;
    std::size_t indexCount_;
    GLuint albedoTex_, normalTex_, roughnessTex_, aoTex_;
};

#endif
