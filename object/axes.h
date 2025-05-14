#ifndef AXES_H
#define AXES_H

#include "../lib/glad.h"
#include <vector>

class Axes {
private:
    unsigned int VAO, VBO;

public:
    Axes() {
        // Define vertices for the XYZ axes
        float vertices[] = {
            // X-axis (red)
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Start point (red)
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // End point (red)

            // Y-axis (green)
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Start point (green)
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // End point (green)

            // Z-axis (blue)
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Start point (blue)
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f  // End point (blue)
        };

        // Generate and bind VAO and VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~Axes() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void render() {
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 6); // Draw 6 vertices (3 lines)
        glBindVertexArray(0);
    }
};

#endif
