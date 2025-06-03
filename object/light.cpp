#include "light.h"
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>

LightSphere::LightSphere(int latSeg, int lonSeg, glm::vec3 col)
    : shader("shaders/light.vs", "shaders/light.fs"), color(col)
{
    buildMesh(latSeg, lonSeg);
}

LightSphere::~LightSphere() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void LightSphere::buildMesh(int latSeg, int lonSeg) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    for(int y = 0; y <= latSeg; ++y) {
        float v = (float)y / latSeg;
        float theta = v * glm::pi<float>();
        for(int x = 0; x <= lonSeg; ++x) {
            float u = (float)x / lonSeg;
            float phi = u * glm::two_pi<float>();
            float px = sin(theta) * cos(phi);
            float py = cos(theta);
            float pz = sin(theta) * sin(phi);
            verts.push_back(px);
            verts.push_back(py);
            verts.push_back(pz);
        }
    }
    for(int y = 0; y < latSeg; ++y) {
        for(int x = 0; x < lonSeg; ++x) {
            int i1 = y * (lonSeg+1) + x;
            int i2 = i1 + lonSeg + 1;
            inds.push_back(i1);
            inds.push_back(i2);
            inds.push_back(i1+1);
            inds.push_back(i1+1);
            inds.push_back(i2);
            inds.push_back(i2+1);
        }
    }
    indexCount = inds.size();
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void LightSphere::Draw(const glm::mat4 &projection,
                       const glm::mat4 &view,
                       const glm::vec3 &position,
                       float dayFactor)
{
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    // make the sphere BIGGER at night, smaller by day:
    float baseSize = 0.3f;
    float nightBump = 2.0f;      // how much bigger at midnight
    float size = glm::mix(baseSize * nightBump, baseSize, dayFactor);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::scale(model, glm::vec3(size));
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
