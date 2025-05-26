#include "terrain.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include "../lib/stb_image.h"

// -----------------------------------------------------------------------------
// Full Diamond–Square on a (2^n + 1) grid
static float getH(float smoothness, std::size_t depth, std::mt19937& gen) {
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    float sign   = dis(gen) > 0 ? 1.0f : -1.0f;
    float reduce = std::pow(2.0f, -smoothness * depth);
    return sign * std::abs(dis(gen)) * reduce;
}

static std::vector<std::vector<float>> generateSquareTerrain(std::size_t size, float smoothness) {
    if (size & (size - 1))
        throw std::invalid_argument("Size must be a power of two.");

    std::size_t N = size + 1;
    std::vector<std::vector<float>> m(N, std::vector<float>(N));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    // initialize corners
    m[0][0]       = dis(gen);
    m[0][size]    = dis(gen);
    m[size][0]    = dis(gen);
    m[size][size] = dis(gen);

    std::size_t numIter = static_cast<std::size_t>(std::log2(size));
    for (std::size_t depth = 1; depth <= numIter; ++depth) {
        std::size_t step = size >> (depth - 1);
        std::size_t half = step >> 1;

        // Diamond
        for (std::size_t y = 0; y < size; y += step) {
            for (std::size_t x = 0; x < size; x += step) {
                float avg = (m[y][x] +
                             m[y][x+step] +
                             m[y+step][x] +
                             m[y+step][x+step]) * 0.25f;
                m[y+half][x+half] = avg + getH(smoothness, depth, gen);
            }
        }

        // Square
        for (std::size_t y = 0; y <= size; y += half) {
            for (std::size_t x = (y + half) % step; x <= size; x += step) {
                float sum = 0.0f; int cnt = 0;
                if (x >= half)           { sum += m[y][x-half];  ++cnt; }
                if (x + half <= size)    { sum += m[y][x+half];  ++cnt; }
                if (y >= half)           { sum += m[y-half][x];  ++cnt; }
                if (y + half <= size)    { sum += m[y+half][x];  ++cnt; }
                m[y][x] = (sum / cnt) + getH(smoothness, depth, gen);
            }
        }
    }

    return m;
}
// -----------------------------------------------------------------------------

// find smallest 2^k >= n
static std::size_t smallestPow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

Terrain::Terrain(std::size_t size, float scale, float heightScale,
                 const std::string& a, const std::string& n,
                 const std::string& r, const std::string& ao,
                 float smooth)
  : size_(size), scale_(scale), heightScale_(heightScale), smoothness_(smooth)
{
    detailNoise_.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailNoise_.SetFrequency(0.2f);

    riverNoise_.SetNoiseType(FastNoiseLite::NoiseType_ValueCubic);
    riverNoise_.SetFrequency(0.0015f);
    riverNoise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    riverNoise_.SetFractalOctaves(4);

    buildMesh();
    loadTexture(a, albedoTex_);
    loadTexture(n, normalTex_);
    loadTexture(r, roughnessTex_);
    loadTexture(ao, aoTex_);
}

Terrain::~Terrain() {
    Cleanup();
    glDeleteTextures(1, &albedoTex_);
    glDeleteTextures(1, &normalTex_);
    glDeleteTextures(1, &roughnessTex_);
    glDeleteTextures(1, &aoTex_);
}

void Terrain::loadTexture(const std::string& path, GLuint& texID) {
    int w,h,channels;
    unsigned char* data = stbi_load(path.c_str(), &w,&h,&channels, 0);
    if (!data) {
        std::cerr<<"Failed to load "<<path<<"\n";
        return;
    }
    GLenum fmt = (channels==3?GL_RGB:GL_RGBA);
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D,texID);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
}

void Terrain::buildMesh() {
    std::size_t gridSize    = size_ + 1;
    float half              = scale_ * 0.5f;
    float uvScale           = 1.f/float(size_);

    // 1) generate full fractal and extract top-left (size_+1)x(size_+1)
    std::size_t squaresize  = smallestPow2(size_);
    auto fractal           = generateSquareTerrain(squaresize, smoothness_);

    std::vector<float> H(gridSize*gridSize);
    for (std::size_t z = 0; z < gridSize; ++z)
      for (std::size_t x = 0; x < gridSize; ++x)
        H[z*gridSize + x] = fractal[z][x] * heightScale_;

    // 2) detail-noise
    for (std::size_t z = 0; z < gridSize; ++z) {
      for (std::size_t x = 0; x < gridSize; ++x) {
        float wx = (x/(float)size_)*scale_ - half;
        float wz = (z/(float)size_)*scale_ - half;
        float& h = H[z*gridSize + x];
        h += detailNoise_.GetNoise(wx, wz)*0.15f*heightScale_;
        h += detailNoise_.GetNoise(wx*4, wz*4)*0.03f*heightScale_;
      }
    }

    // 3) build verts
    struct V{glm::vec3 p;glm::vec2 uv;glm::vec3 n;};
    std::vector<V> verts(gridSize*gridSize);
    for (std::size_t z=0;z<gridSize;++z) {
      for (std::size_t x=0;x<gridSize;++x) {
        auto& v = verts[z*gridSize + x];
        v.p = {(x/(float)size_)*scale_-half, H[z*gridSize+x], (z/(float)size_)*scale_-half};
        v.uv = {x*uvScale, z*uvScale};
      }
    }
    addRivers(*(std::vector<Vertex>*)&verts);  // safe cast: same memory layout

    // 4) normals
    for (std::size_t z=0;z<gridSize;++z) {
      for (std::size_t x=0;x<gridSize;++x) {
        float hl = verts[z*gridSize + (x?x-1:x)].p.y;
        float hr = verts[z*gridSize + ((x+1<gridSize)?x+1:x)].p.y;
        float hd = verts[((z?z-1:z))*gridSize + x].p.y;
        float hu = verts[((z+1<gridSize)?z+1:z)*gridSize + x].p.y;
        glm::vec3 tan{2, hr-hl, 0}, bit{0, hu-hd, 2};
        verts[z*gridSize + x].n = glm::normalize(glm::cross(tan,bit));
      }
    }

    // 5) indices
    std::vector<GLuint> idx;
    idx.reserve(size_*size_*6);
    for (std::size_t z=0; z<size_; ++z) {
      for (std::size_t x=0; x<size_; ++x) {
        GLuint tl=z*gridSize+x, tr=tl+1, bl=(z+1)*gridSize+x, br=bl+1;
        idx.insert(idx.end(),{tl,bl,tr,tr,bl,br});
      }
    }
    indexCount_ = idx.size();

    // 6) GPU
    glGenVertexArrays(1,&vao_);
    glGenBuffers(1,&vbo_);
    glGenBuffers(1,&ebo_);

    glBindVertexArray(vao_);
      glBindBuffer(GL_ARRAY_BUFFER,vbo_);
      glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(V),verts.data(),GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(GLuint),idx.data(),GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void Terrain::addRivers(std::vector<Vertex>& verts) {
    float rd = heightScale_*1.2f;
    for (auto& v : verts) {
      float m = std::pow(1.0f - std::abs(riverNoise_.GetNoise(v.position.x,v.position.z)*1.5f),5.0f);
      if (m>0.25f) {
        float b=(m-0.25f)/0.75f;
        v.position.y -= rd*b;
        v.position.y += rd*0.1f*detailNoise_.GetNoise(v.position.x*3,v.position.z*3);
      }
    }
}

void Terrain::Draw() {
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,albedoTex_);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,normalTex_);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,roughnessTex_);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,aoTex_);

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES,(GLsizei)indexCount_,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}

void Terrain::Cleanup() {
    glDeleteVertexArrays(1,&vao_);
    glDeleteBuffers(1,&vbo_);
    glDeleteBuffers(1,&ebo_);
}
