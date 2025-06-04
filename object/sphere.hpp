#pragma once
#include <vector>
#include "../lib/glad.h"

struct Sphere {
    // OpenGL handles
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;

    // Call once after GL context is ready:
    void build(unsigned int sectorCount = 32, unsigned int stackCount = 16);

    // Call each frame when drawing:
    void draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};