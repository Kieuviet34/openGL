#ifndef LIGHT_H
#define LIGHT_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"

class LightSphere {
public:
    // construct a UV-sphere mesh with given subdivisions and color
    LightSphere(int latSegments = 16, int lonSegments = 16, glm::vec3 color = glm::vec3(1.0f));
    ~LightSphere();

    // render at given position with provided projection/view
    void Draw(const glm::mat4 &projection,
              const glm::mat4 &view,
              const glm::vec3 &position);

private:
    unsigned int VAO, VBO, EBO;
    size_t indexCount;
    Shader shader;        // simple color shader
    glm::vec3 color;

    void buildMesh(int latSeg, int lonSeg);
};

#endif  

