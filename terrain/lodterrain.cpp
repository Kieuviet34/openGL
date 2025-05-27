#include "lodterrain.h"
#include <glm/glm.hpp>
#include "../lib/FastNoiseLite.h"
#include "../lib/stb_image.h"
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <future>
#include <thread>

#ifndef WATER_HEIGHT
#define WATER_HEIGHT 4.5f
#endif

struct MeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<GLuint> indices;
};

// Diamond-Square helper (fixed seed for reproducibility)
static float getH(float s, std::size_t d, std::mt19937& g) {
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    float sign = dis(g) > 0 ? 1.0f : -1.0f;
    float reduce = std::pow(2.0f, -s * float(d));
    return sign * std::abs(dis(g)) * reduce;
}

static std::vector<std::vector<float>> generateSquareTerrain(std::size_t size, float smoothness) {
    if (size & (size - 1)) throw std::invalid_argument("Size must be a power of two.");
    std::size_t N = size + 1;
    std::vector<std::vector<float>> m(N, std::vector<float>(N, 0.0f));
    std::mt19937 gen(1337);
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    m[0][0] = dis(gen);
    m[0][size] = dis(gen);
    m[size][0] = dis(gen);
    m[size][size] = dis(gen);

    std::size_t steps = std::log2(size);
    for (std::size_t depth = 1; depth <= steps; ++depth) {
        std::size_t step = size >> (depth - 1);
        std::size_t half = step >> 1;
        // Diamond step
        for (std::size_t y = 0; y < size; y += step) {
            for (std::size_t x = 0; x < size; x += step) {
                float avg = (m[y][x] + m[y][x+step] + m[y+step][x] + m[y+step][x+step]) * 0.25f;
                m[y+half][x+half] = avg + getH(smoothness, depth, gen);
            }
        }
        // Square step
        for (std::size_t y = 0; y <= size; y += half) {
            for (std::size_t x = (y+half) % step; x <= size; x += step) {
                float sum = 0; int cnt = 0;
                if (x >= half)        { sum += m[y][x-half];  ++cnt; }
                if (x+half <= size)   { sum += m[y][x+half];  ++cnt; }
                if (y >= half)        { sum += m[y-half][x];  ++cnt; }
                if (y+half <= size)   { sum += m[y+half][x];  ++cnt; }
                m[y][x] = sum/cnt + getH(smoothness, depth, gen);
            }
        }
    }
    return m;
}

static std::size_t smallestPow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// Constructor
LodTerrain::LodTerrain(std::size_t tileSize, int lodLevels,
                      float scale, float heightScale, float smoothness,
                      const std::string& albedoPath, const std::string& normalPath,
                      const std::string& roughnessPath, const std::string& aoPath)
    : lodLevels_(lodLevels)
    , tileSize_(tileSize)
    , scale_(scale)
    , heightScale_(heightScale)
    , smoothness_(smoothness)
    , yOffset_(WATER_HEIGHT)
{
    initializeNoise();
    initializeTextures(albedoPath, normalPath, roughnessPath, aoPath);
    initializeTiles();
}

// Initialize noise generators
void LodTerrain::initializeNoise() {
    detailNoise_.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailNoise_.SetFrequency(0.25f);
    riverNoise_.SetNoiseType(FastNoiseLite::NoiseType_ValueCubic);
    riverNoise_.SetFrequency(0.0035f);
}

// Load all textures
void LodTerrain::initializeTextures(const std::string& albedoPath,
                                   const std::string& normalPath,
                                   const std::string& roughnessPath,
                                   const std::string& aoPath) {
    loadTexture(albedoPath, albedoTex_);
    loadTexture(normalPath, normalTex_);
    loadTexture(roughnessPath, roughnessTex_);
    loadTexture(aoPath, aoTex_);
}

// Initialize base tile and LODs
void LodTerrain::initializeTiles() {
    Tile base;
    base.pos = glm::vec3(-scale_ * 0.5f, yOffset_, -scale_ * 0.5f);
    std::size_t N = smallestPow2(tileSize_);
    base.gridSize = N + 1;
    auto fractal = generateSquareTerrain(N, smoothness_);
    base.heightmap.resize((N+1)*(N+1));
    for (std::size_t j = 0; j <= N; ++j)
        for (std::size_t i = 0; i <= N; ++i)
            base.heightmap[j*(N+1)+i] = fractal[j][i];
    tiles_.push_back(std::move(base));
    generateLODs(tiles_[0]);
}

// Generate LOD meshes using multithreading
void LodTerrain::generateLODs(Tile& tile) {
    std::vector<std::future<MeshData>> futures;
    futures.reserve(lodLevels_);
    for (int lvl = 0; lvl < lodLevels_; ++lvl) {
        std::size_t res = std::max<std::size_t>(tileSize_ >> lvl, 2u);
        futures.emplace_back(std::async(std::launch::async,
            [this, res, &tile]() -> MeshData {
                MeshData md;
                std::size_t N2 = tile.gridSize - 1;
                float step = scale_ / (res-1);
                md.positions.reserve(res*res);
                md.uvs.reserve(res*res);
                md.normals.reserve(res*res);
                for (std::size_t z = 0; z < res; ++z) {
                    for (std::size_t x = 0; x < res; ++x) {
                        float u = float(x)/(res-1), v = float(z)/(res-1);
                        std::size_t i = std::min<std::size_t>(std::round(u*N2), N2);
                        std::size_t j = std::min<std::size_t>(std::round(v*N2), N2);
                        float wx = tile.pos.x + x*step;
                        float wz = tile.pos.z + z*step;
                        float h = tile.heightmap[j*(N2+1)+i] * heightScale_;
                        md.positions.emplace_back(wx, h + tile.pos.y, wz);
                        md.uvs.emplace_back(u, v);
                    }
                }
                for (std::size_t z = 0; z < res; ++z) {
                    for (std::size_t x = 0; x < res; ++x) {
                        int idx0 = z*res + x;
                        auto sampleY = [&](int dx, int dz) {
                            int xi = std::clamp<int>(x+dx, 0, res-1);
                            int zi = std::clamp<int>(z+dz, 0, res-1);
                            return md.positions[zi*res + xi].y;
                        };
                        float hl = sampleY(-1, 0), hr = sampleY(+1, 0);
                        float hd = sampleY(0, -1), hu = sampleY(0, +1);
                        glm::vec3 tan{step*2, hr-hl, 0}, bit{0, hu-hd, step*2};
                        md.normals.push_back(glm::normalize(glm::cross(tan, bit)));
                    }
                }
                md.indices.reserve((res-1)*(res-1)*6);
                for (std::size_t z = 0; z+1 < res; ++z) {
                    for (std::size_t x = 0; x+1 < res; ++x) {
                        GLuint tl = z*res + x;
                        GLuint tr = tl + 1;
                        GLuint bl = (z+1)*res + x;
                        GLuint br = bl + 1;
                        md.indices.insert(md.indices.end(),
                            { tl, bl, tr, tr, bl, br });
                    }
                }
                return md;
            }));
    }
    tile.lods.reserve(lodLevels_);
    for (int lvl = 0; lvl < lodLevels_; ++lvl) {
        MeshData md = futures[lvl].get();
        TileLOD lod;
        buildTileMesh(lod, tile, std::max<std::size_t>(tileSize_ >> lvl, 2u));
        lod.indexCount = md.indices.size();
        lod.center = tile.pos + glm::vec3(scale_ * 0.5f, 0, scale_ * 0.5f);
        lod.radius = glm::length(glm::vec3(scale_ * 0.5f, heightScale_, scale_ * 0.5f));
        tile.lods.push_back(std::move(lod));
    }
}

// Build and upload mesh data to GPU
void LodTerrain::buildTileMesh(TileLOD& lod, const Tile& tile, std::size_t resolution) {
    MeshData md;
    std::size_t res = resolution;
    float step = scale_ / (res-1);
    std::size_t N2 = tile.gridSize - 1;
    md.positions.reserve(res*res);
    md.uvs.reserve(res*res);
    md.normals.reserve(res*res);
    for (std::size_t z = 0; z < res; ++z) {
        for (std::size_t x = 0; x < res; ++x) {
            float u = float(x)/(res-1), v = float(z)/(res-1);
            std::size_t i = std::min<std::size_t>(std::round(u*N2), N2);
            std::size_t j = std::min<std::size_t>(std::round(v*N2), N2);
            float wx = tile.pos.x + x*step;
            float wz = tile.pos.z + z*step;
            float h = tile.heightmap[j*(N2+1)+i] * heightScale_;
            md.positions.emplace_back(wx, h + tile.pos.y, wz);
            md.uvs.emplace_back(u, v);
        }
    }
    for (std::size_t z = 0; z < res; ++z) {
        for (std::size_t x = 0; x < res; ++x) {
            auto sampleY = [&](int dx, int dz) {
                int xi = std::clamp<int>(x+dx, 0, res-1);
                int zi = std::clamp<int>(z+dz, 0, res-1);
                return md.positions[zi*res + xi].y;
            };
            float hl = sampleY(-1, 0), hr = sampleY(+1, 0);
            float hd = sampleY(0, -1), hu = sampleY(0, +1);
            glm::vec3 tan{step*2, hr-hl, 0}, bit{0, hu-hd, step*2};
            md.normals.push_back(glm::normalize(glm::cross(tan, bit)));
        }
    }
    md.indices.reserve((res-1)*(res-1)*6);
    for (std::size_t z = 0; z+1 < res; ++z) {
        for (std::size_t x = 0; x+1 < res; ++x) {
            GLuint tl = z*res + x;
            GLuint tr = tl + 1;
            GLuint bl = (z+1)*res + x;
            GLuint br = bl + 1;
            md.indices.insert(md.indices.end(),
                { tl, bl, tr, tr, bl, br });
        }
    }
    glGenVertexArrays(1, &lod.vao);
    glGenBuffers(1, &lod.vbo);
    glGenBuffers(1, &lod.ebo);
    glBindVertexArray(lod.vao);
    struct V { glm::vec3 p; glm::vec2 uv; glm::vec3 n; };
    std::vector<V> verts(md.positions.size());
    for (size_t k = 0; k < verts.size(); ++k) {
        verts[k].p = md.positions[k];
        verts[k].uv = md.uvs[k];
        verts[k].n = md.normals[k];
    }
    glBindBuffer(GL_ARRAY_BUFFER, lod.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lod.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, md.indices.size() * sizeof(GLuint), md.indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

// Load texture
void LodTerrain::loadTexture(const std::string& path, GLuint& texID) {
    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return;
    }
    GLenum format = (channels == 3 ? GL_RGB : GL_RGBA);
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
}

// Draw terrain
void LodTerrain::Draw(const glm::vec3& camPos) {
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, albedoTex_);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, normalTex_);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, roughnessTex_);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, aoTex_);
    for (auto& tile : tiles_) {
        float d = glm::distance(camPos, tile.pos + glm::vec3(scale_ * 0.5f, 0, scale_ * 0.5f));
        int lvl = 0;
        float thresh = scale_;
        while (lvl + 1 < lodLevels_ && d > thresh) {
            d -= thresh;
            thresh *= 2;
            ++lvl;
        }
        auto& L = tile.lods[lvl];
        glBindVertexArray(L.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)L.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

// Cleanup resources
void LodTerrain::Cleanup() {
    for (auto& t : tiles_) {
        for (auto& L : t.lods) {
            glDeleteVertexArrays(1, &L.vao);
            glDeleteBuffers(1, &L.vbo);
            glDeleteBuffers(1, &L.ebo);
        }
    }
    tiles_.clear();
}

// Destructor
LodTerrain::~LodTerrain() {
    Cleanup();
    glDeleteTextures(1, &albedoTex_);
    glDeleteTextures(1, &normalTex_);
    glDeleteTextures(1, &roughnessTex_);
    glDeleteTextures(1, &aoTex_);
}