#ifndef LOD_TERRAIN_H
#define LOD_TERRAIN_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../lib/glad.h"
#include "../lib/FastNoiseLite.h"

class LodTerrain {
public:
    // tileSize: base heightmap resolution (power of two)
    // lodLevels: number of LOD levels to generate
    // scale: world‐space width/depth of the terrain tile
    // heightScale: vertical exaggeration
    // smoothness: diamond–square parameter
    LodTerrain(std::size_t tileSize,
               int          lodLevels,
               float        scale,
               float        heightScale,
               float        smoothness,
               const std::string& albedoPath,
               const std::string& normalPath);
    ~LodTerrain();

    // Draws whichever LOD is appropriate for camPos
    // Assumes you've already bound and set your terrain shader
    // and updated its uniforms (model/view/proj, fog, light, etc.)
    void Draw(const glm::vec3& camPos);

    // exposes the two loaded textures:
    GLuint albedoTex() const { return _albedo; }
    GLuint normalTex() const { return _normal; }
    float getHeightAt(float worldX, float worldZ) const;
    glm::vec3 getNormalAt(float worldX, float worldZ) const;

private:
    struct TileLOD {
        GLuint vao=0, vbo=0, ebo=0;
        GLsizei indexCount=0;
        glm::vec3 center;
    };

    struct Tile {
        glm::vec3      origin;
        std::vector<TileLOD> lods;
        std::vector<float>   heightmap; // size (N+1)*(N+1)
        std::size_t          gridSize;  // =N+1
    };

    std::vector<Tile>    _tiles;
    int                  _lodLevels;
    std::size_t          _tileSize;
    float                _scale, _heightScale, _smoothness, _yOffset;

    // only two textures now
    GLuint               _albedo, _normal;

    FastNoiseLite        _detailNoise;

    // init steps:
    void initializeNoise();
    void initializeTextures(const std::string& a, const std::string& n);
    void initializeTiles();

    void generateLODs(Tile& tile);
    void buildTileMesh(Tile& tile, TileLOD& lod, std::size_t resolution);
    void loadTexture(const std::string& path, GLuint& texID);
};

#endif // LOD_TERRAIN_H
