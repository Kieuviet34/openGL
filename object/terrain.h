#ifndef TERRAIN_H
#define TERRAIN_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>
#include "../lib/FastNoiseLite.h"
#include <string>
#include <algorithm> // For std::clamp

class Terrain {
public:
    Terrain(std::size_t size, float scale, float heightScale, 
            const std::string& albedoPath, const std::string& normalPath, 
            const std::string& roughnessPath, const std::string& aoPath,
            FastNoiseLite::NoiseType type = FastNoiseLite::NoiseType_OpenSimplex2S);
    ~Terrain();

    void Draw();
    void Cleanup();
    std::size_t GetGridSize() const { return size_; }
    float getScale() const { return scale_; }
    float getHeight() const { return heightScale_; }

private:
    struct Vertex {
            glm::vec3 position;
            glm::vec2 texCoord;
            glm::vec3 normal;
        };
    void buildMesh();
    void loadTexture(const std::string& path, GLuint& textureID);
    void addRivers(std::vector<Vertex>& verts);
    void applyHeightModifications(float& h, float wx, float wz);

    std::size_t size_;
    float scale_;
    float heightScale_;
    FastNoiseLite noise_;
    FastNoiseLite detailNoise_;
    FastNoiseLite riverNoise_;

    GLuint vao_, vbo_, ebo_;
    std::size_t indexCount_;
    GLuint albedoTex_, normalTex_, roughnessTex_, aoTex_;
    
    
};
#endif