#include "terrain.h"
#include <glm/gtc/constants.hpp>
#include <iostream>
#include "../lib/stb_image.h"

Terrain::Terrain(std::size_t size, float scale, float heightScale, 
                const std::string& albedoPath, const std::string& normalPath, 
                const std::string& roughnessPath, const std::string& aoPath,
                FastNoiseLite::NoiseType type)
    : size_(size), scale_(scale), heightScale_(heightScale), 
      albedoTex_(0), normalTex_(0), roughnessTex_(0), aoTex_(0)
{
    // Main noise configuration
    noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise_.SetFrequency(0.008f);
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalOctaves(4);
    noise_.SetFractalLacunarity(1.8f);
    noise_.SetFractalGain(0.5f);
    noise_.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    noise_.SetDomainWarpAmp(25.0f);

    // Detail noise for small features
    detailNoise_.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailNoise_.SetFrequency(0.25f);
    detailNoise_.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Hybrid);

    // River noise
    riverNoise_.SetNoiseType(FastNoiseLite::NoiseType_ValueCubic);
    riverNoise_.SetFrequency(0.0035f);

    buildMesh();
    loadTexture(albedoPath, albedoTex_);
    loadTexture(normalPath, normalTex_);
    loadTexture(roughnessPath, roughnessTex_);
    loadTexture(aoPath, aoTex_);
}

Terrain::~Terrain() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ebo_);
    glDeleteTextures(1, &albedoTex_);
    glDeleteTextures(1, &normalTex_);
    glDeleteTextures(1, &roughnessTex_);
    glDeleteTextures(1, &aoTex_);
    Cleanup();
}

void Terrain::loadTexture(const std::string& path, GLuint& textureID) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Terrain::buildMesh() {
    std::vector<Vertex> verts((size_ + 1) * (size_ + 1));
    const float half = scale_ * 0.5f;
    const float uvScale = 0.08f; // Texture tiling factor

    for (std::size_t z = 0; z <= size_; ++z) {
        for (std::size_t x = 0; x <= size_; ++x) {
            float wx = (float(x)/size_)*scale_ - half;
            float wz = (float(z)/size_)*scale_ - half;
            
            // Base height with domain warp
            noise_.DomainWarp(wx, wz);
            float h = noise_.GetNoise(wx, wz);
            
            // Apply height modifications
            applyHeightModifications(h, wx, wz);
            
            // Calculate position and texture coordinates
            Vertex vertex;
            vertex.position = {wx, h * heightScale_, wz};
            vertex.texCoord = {x * uvScale, z * uvScale};

            // Calculate normals
            const float eps = scale_ / size_;
            const float hL = noise_.GetNoise(wx - eps, wz) * heightScale_;
            const float hR = noise_.GetNoise(wx + eps, wz) * heightScale_;
            const float hD = noise_.GetNoise(wx, wz - eps) * heightScale_;
            const float hU = noise_.GetNoise(wx, wz + eps) * heightScale_;
            
            const glm::vec3 tangent(2.0f, hR - hL, 0.0f);
            const glm::vec3 bitangent(0.0f, hU - hD, 2.0f);
            vertex.normal = glm::normalize(glm::cross(tangent, bitangent));

            verts[z * (size_ + 1) + x] = vertex;
        }
    }

    addRivers(verts); 
    std::vector<GLuint> idx;
    idx.reserve(size_ * size_ * 6);
    for (std::size_t z = 0; z < size_; ++z) {
        for (std::size_t x = 0; x < size_; ++x) {
            GLuint tl = z * (size_ + 1) + x;
            GLuint tr = tl + 1;
            GLuint bl = (z + 1) * (size_ + 1) + x;
            GLuint br = bl + 1;
            idx.push_back(tl);
            idx.push_back(bl);
            idx.push_back(tr);
            idx.push_back(tr);
            idx.push_back(bl);
            idx.push_back(br);
        }
    }
    indexCount_ = idx.size();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Texture coordinate attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, texCoord));

    // Normal attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, normal));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}
void Terrain::addRivers(std::vector<Vertex>& verts) {
    
    const float riverDepth = heightScale_ * 0.7f;
    
    for(auto& v : verts) {
        const float river = riverNoise_.GetNoise(v.position.x, v.position.z);
        const float riverMask = std::pow(1.0f - std::abs(river * 2.5f), 3.0f);
        
        if(riverMask > 0.35f) {
            const float blend = (riverMask - 0.35f) / 0.65f;
            v.position.y -= riverDepth * blend;
            
            // Smooth river banks
            v.position.y += riverDepth * 0.2f * 
                detailNoise_.GetNoise(v.position.x * 5.0f, v.position.z * 5.0f);
        }
    }
}
void Terrain::applyHeightModifications(float& h, float wx, float wz) {
    // Base shape with smooth hills
    h = sin(h * 0.8f) * 1.2f;
    
    // Add medium-scale detail
    h += detailNoise_.GetNoise(wx * 1.2f, wz * 1.2f) * 0.15f;
    
    // Valley formation with smooth transition
    if(h < -0.1f) {
        h = -0.1f + (h + 0.15f) * 0.4f;
    }
    
    // Height clamping
    h = std::clamp(h, -.9f, .9f);
}
void Terrain::Draw() {
    // Bind all textures to different texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedoTex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTex_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, roughnessTex_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, aoTex_);

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // Unbind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void Terrain::Cleanup() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ebo_);
    glDeleteTextures(1, &albedoTex_);
    glDeleteTextures(1, &normalTex_);
    glDeleteTextures(1, &roughnessTex_);
    glDeleteTextures(1, &aoTex_);
    
    // Reset IDs to prevent accidental reuse
    vao_ = vbo_ = ebo_ = 0;
    albedoTex_ = normalTex_ = roughnessTex_ = aoTex_ = 0;
}
