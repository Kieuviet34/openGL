#ifndef LOD_TERRAIN_H
#define LOD_TERRAIN_H

#include "terrain.h"
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include "../lib/glad.h"
#include "../lib/FastNoiseLite.h"

class LodTerrain {
public:
    LodTerrain(std::size_t tileSize,
               int lodLevels,
               float scale,
               float heightScale,
               float smoothness,
               const std::string& albedoPath,
               const std::string& normalPath,
               const std::string& roughnessPath,
               const std::string& aoPath);
    ~LodTerrain();

    void Draw(const glm::vec3& camPos);
    void Cleanup();

private:
    struct TileLOD {
        GLuint vao, vbo, ebo;
        std::size_t indexCount;
        glm::vec3 center;
        float radius;
    };

    struct Tile {
        glm::vec3 pos;
        std::vector<TileLOD> lods;
        std::vector<float> heightmap;
        std::size_t gridSize;
    };

    std::vector<Tile> tiles_;
    int lodLevels_;
    std::size_t tileSize_;
    float scale_, heightScale_, smoothness_;
    float yOffset_;
    GLuint albedoTex_, normalTex_, roughnessTex_, aoTex_;
    FastNoiseLite detailNoise_, riverNoise_;

    // Initialization methods
    void initializeNoise();
    void initializeTextures(const std::string& albedoPath,
                           const std::string& normalPath,
                           const std::string& roughnessPath,
                           const std::string& aoPath);
    void initializeTiles();
    void generateLODs(Tile& tile);
    void buildTileMesh(TileLOD& lod, const Tile& tile, std::size_t resolution);
    void loadTexture(const std::string& path, GLuint& texID);
};

#endif // LOD_TERRAIN_H