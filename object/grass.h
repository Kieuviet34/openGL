#ifndef GRASS_H
#define GRASS_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

class Grass
{
public:
    unsigned int VAO, VBO;
    unsigned int texture;
    std::vector<glm::vec3> positions;

    Grass(const char* texturePath, const std::vector<glm::vec3>& vegetationPositions);
    void Draw(unsigned int shaderProgramID);

private:
    void setupMesh();
    unsigned int loadTexture(const char* path);
};

#endif
