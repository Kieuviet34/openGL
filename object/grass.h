#ifndef GRASS_H
#define GRASS_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Grass
{
public:
    Grass(const char* texturePath, const std::vector<glm::vec3>& vegetationPositions);

    /// Draws all grass blades.  
    /// @param shaderID  the GLSL program ID (must already have set view, projection, texture1)  
    /// @param view      current view matrix  
    /// @param cameraPos world‑space camera position  
    void Draw(unsigned int shaderID,
              const glm::mat4& view,
              const glm::mat4& projection,
              const glm::vec3& cameraPos);

private:
    void setupMesh();
    unsigned int loadTexture(const char* path);

    unsigned int VAO, VBO;
    unsigned int textureID;
    std::vector<glm::vec3> positions;
};
#endif
