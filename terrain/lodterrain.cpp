#include "lodterrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include "../lib/stb_image.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef WATER_HEIGHT
#define WATER_HEIGHT 10.5f   
#endif

// -----------------------------------------------------------------------------
// Diamond–Square helper
static float getH(float s, std::size_t d, std::mt19937& g) {
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    float sign   = dis(g) > 0 ? 1.0f : -1.0f;
    float reduce = std::pow(2.0f, -s * float(d));
    return sign * std::abs(dis(g)) * reduce;
}

static std::vector<std::vector<float>> generateSquareTerrain(std::size_t size, float smoothness) {
    if (size & (size - 1))
        throw std::invalid_argument("Size must be a power of two.");

    std::size_t N = size + 1;
    std::vector<std::vector<float>> m(N, std::vector<float>(N, 0.0f));
    std::mt19937 gen(1337);
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    // initialize corners
    m[0][0]       = dis(gen);
    m[0][size]    = dis(gen);
    m[size][0]    = dis(gen);
    m[size][size] = dis(gen);

    std::size_t steps = static_cast<std::size_t>(std::log2(size));
    for (std::size_t depth = 1; depth <= steps; ++depth) {
        std::size_t step = size >> (depth - 1);
        std::size_t half = step >> 1;

        // Diamond step
        for (std::size_t y = 0; y < size; y += step) {
            for (std::size_t x = 0; x < size; x += step) {
                float avg = (m[y][x]
                           + m[y][x + step]
                           + m[y + step][x]
                           + m[y + step][x + step]) * 0.25f;
                m[y + half][x + half] = avg + getH(smoothness, depth, gen);
            }
        }

        // Square step
        for (std::size_t y = 0; y <= size; y += half) {
            for (std::size_t x = (y + half) % step; x <= size; x += step) {
                float sum = 0.0f; int cnt = 0;
                if (x >= half)        { sum += m[y][x-half];  ++cnt; }
                if (x+half <= size)   { sum += m[y][x+half];  ++cnt; }
                if (y >= half)        { sum += m[y-half][x];  ++cnt; }
                if (y+half <= size)   { sum += m[y+half][x];  ++cnt; }
                m[y][x] = (sum / cnt) + getH(smoothness, depth, gen);
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

// -----------------------------------------------------------------------------
// Constructor
LodTerrain::LodTerrain(std::size_t tileSize, int lodLevels,
                       float scale, float heightScale, float smoothness,
                       const std::string& a, const std::string& n,
                       const std::string& r, const std::string& ao)
  : lodLevels_(lodLevels)
  , tileSize_(tileSize)
  , scale_(scale)
  , heightScale_(heightScale)
  , smoothness_(smoothness)
  , yOffset_(WATER_HEIGHT)
{
    detailNoise_.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailNoise_.SetFrequency(0.25f);

    riverNoise_.SetNoiseType(FastNoiseLite::NoiseType_ValueCubic);
    riverNoise_.SetFrequency(0.0035f);

    loadTexture(a, albedoTex_);
    loadTexture(n, normalTex_);
    loadTexture(r, roughnessTex_);
    loadTexture(ao, aoTex_);

    // Build base tile
    Tile t;
    t.pos = {0.0f, yOffset_, 0.0f};
    std::size_t N = smallestPow2(tileSize_);
    t.gridSize = N + 1;

    // Generate fractal
    auto fractal = generateSquareTerrain(N, smoothness_);

    // Copy into heightmap and find min
    t.heightmap.resize((N+1)*(N+1));
    float minH = std::numeric_limits<float>::infinity();
    for (std::size_t j = 0; j <= N; ++j) {
        for (std::size_t i = 0; i <= N; ++i) {
            float v = fractal[j][i];
            t.heightmap[j*(N+1) + i] = v;
            minH = std::min(minH, v);
        }
    }
    // Shift so that lowest point becomes 0
    for (auto &v : t.heightmap) {
        v = (v - minH);
    }

    tiles_.push_back(std::move(t));
    generateLODs(tiles_.back());
}

LodTerrain::~LodTerrain() {
    Cleanup();
    glDeleteTextures(1, &albedoTex_);
    glDeleteTextures(1, &normalTex_);
    glDeleteTextures(1, &roughnessTex_);
    glDeleteTextures(1, &aoTex_);
}

void LodTerrain::generateLODs(Tile& tile) {
    for (int lvl = 0; lvl < lodLevels_; ++lvl) {
        TileLOD lod;
        std::size_t res = std::max<std::size_t>(tileSize_ >> lvl, 2u);
        buildTileMesh(lod, tile, res);
        lod.center = tile.pos + glm::vec3(scale_*0.5f, 0.0f, scale_*0.5f);
        lod.radius = glm::length(glm::vec3(scale_*0.5f, heightScale_, scale_*0.5f));
        tile.lods.push_back(lod);
    }
}

void LodTerrain::buildTileMesh(TileLOD& lod, const Tile& tile, std::size_t res) {
    struct V { glm::vec3 p, n; glm::vec2 uv; };
    std::vector<V> verts; verts.reserve(res*res);
    std::vector<GLuint> idx; idx.reserve((res-1)*(res-1)*6);

    float step = scale_ / (res - 1);
    std::size_t N = tile.gridSize - 1;

    for (std::size_t z = 0; z < res; ++z) {
        for (std::size_t x = 0; x < res; ++x) {
            float u = x / float(res-1), v = z / float(res-1);
            std::size_t i = std::min<std::size_t>(std::round(u*N), N),
                         j = std::min<std::size_t>(std::round(v*N), N);

            float wx = tile.pos.x + x*step,
                  wz = tile.pos.z + z*step;
            // heightmap đã shift sẵn, ≥0
            float h = tile.heightmap[j*(N+1)+i] * heightScale_;

            V vert;
            vert.p  = { wx, h + tile.pos.y, wz };  // pos.y == yOffset_
            vert.uv = { u, v };

            // river carve
            float m = std::pow(1.0f - std::abs(riverNoise_.GetNoise(wx, wz)*1.5f), 5.0f);
            if (m > 0.25f) {
                float b  = (m - 0.25f) / 0.75f;
                float rd = heightScale_ * 1.2f;
                vert.p.y -= rd*b;
                vert.p.y += rd*0.1f * detailNoise_.GetNoise(wx*3, wz*3);
            }

            // normals
            auto sampleH = [&](int di, int dj){
                int ii = std::clamp<int>(i+di, 0, N),
                    jj = std::clamp<int>(j+dj, 0, N);
                return tile.heightmap[jj*(N+1)+ii] * heightScale_;
            };
            float hl = sampleH(-1,0), hr = sampleH(+1,0),
                  hd = sampleH(0,-1), hu = sampleH(0,+1);
            glm::vec3 tan{ step*2, hr-hl, 0 },
                      bit{ 0, hu-hd, step*2 };
            vert.n = glm::normalize(glm::cross(tan, bit));

            verts.push_back(vert);
        }
    }

    // indices
    for (std::size_t z = 0; z+1 < res; ++z) {
        for (std::size_t x = 0; x+1 < res; ++x) {
            GLuint tl = z*res + x,
                   tr = tl+1,
                   bl = (z+1)*res + x,
                   br = bl+1;
            idx.insert(idx.end(), { tl, bl, tr, tr, bl, br });
        }
    }

    lod.indexCount = idx.size();
    glGenVertexArrays(1, &lod.vao);
    glGenBuffers(1, &lod.vbo);
    glGenBuffers(1, &lod.ebo);

    glBindVertexArray(lod.vao);
      glBindBuffer(GL_ARRAY_BUFFER, lod.vbo);
      glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(V), verts.data(), GL_STATIC_DRAW);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,n));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,uv));

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lod.ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void LodTerrain::loadTexture(const std::string& path, GLuint& texID) {
    int w,h,c;
    auto* d = stbi_load(path.c_str(), &w, &h, &c, 0);
    if (!d) { std::cerr<<"Failed "<<path<<"\n"; return; }
    GLenum fmt = (c==3?GL_RGB:GL_RGBA);
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,d);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(d);
}

void LodTerrain::Draw(const glm::vec3& camPos) {
    glActiveTexture(GL_TEXTURE0);  glBindTexture(GL_TEXTURE_2D, albedoTex_);
    glActiveTexture(GL_TEXTURE1);  glBindTexture(GL_TEXTURE_2D, normalTex_);
    glActiveTexture(GL_TEXTURE2);  glBindTexture(GL_TEXTURE_2D, roughnessTex_);
    glActiveTexture(GL_TEXTURE3);  glBindTexture(GL_TEXTURE_2D, aoTex_);

    for (auto& tile : tiles_) {
        float d = glm::distance(camPos, tile.pos + glm::vec3(scale_*0.5f,0,scale_*0.5f));
        int lvl=0; float thresh=scale_;
        while (lvl+1<lodLevels_ && d>thresh) { d-=thresh; thresh*=2; ++lvl; }
        auto& L = tile.lods[lvl];
        glBindVertexArray(L.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)L.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

void LodTerrain::Cleanup() {
    for (auto& t : tiles_) {
        for (auto& L : t.lods) {
            glDeleteVertexArrays(1,&L.vao);
            glDeleteBuffers(1,&L.vbo);
            glDeleteBuffers(1,&L.ebo);
        }
    }
    tiles_.clear();
}
