#ifndef GROUND_H
#define GROUND_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"
#include <string>

class Ground {
private:
    unsigned int groundVAO, groundVBO;
    unsigned int textureID;
    unsigned int loadTexture(const char* path);

    // Vertex data: position (3), normal (3), texcoord (2), tangent (3), bitangent (3)
    // Total: 14 floats per vertex, 6 vertices
    static constexpr float groundVertices[6 * 14] = {
        // triangle 1
         15.f, -0.5f,  15.f,   0.f,1.f,0.f,   2.f,0.f,    1.f,0.f,0.f,    0.f,0.f,1.f,
        -15.f, -0.5f,  15.f,   0.f,1.f,0.f,   0.f,0.f,    1.f,0.f,0.f,    0.f,0.f,1.f,
        -15.f, -0.5f, -15.f,   0.f,1.f,0.f,   0.f,2.f,    1.f,0.f,0.f,    0.f,0.f,1.f,
        // triangle 2
         15.f, -0.5f,  15.f,   0.f,1.f,0.f,   2.f,0.f,    1.f,0.f,0.f,    0.f,0.f,1.f,
        -15.f, -0.5f, -15.f,   0.f,1.f,0.f,   0.f,2.f,    1.f,0.f,0.f,    0.f,0.f,1.f,
         15.f, -0.5f, -15.f,   0.f,1.f,0.f,   2.f,2.f,    1.f,0.f,0.f,    0.f,0.f,1.f
    };

public:
    Ground(const char* texturePath);
    void Draw(Shader &shader);
    unsigned int getVAO() const { return groundVAO; }
    unsigned int getTextureID() const { return textureID; }
};

#endif