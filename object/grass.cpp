// grass.cpp
#include "grass.h"
#include "../lib/stb_image.h"
#include <iostream>

Grass::Grass(const char* texturePath,
             const std::vector<glm::vec3>& vegetationPositions)
    : positions(vegetationPositions)
{
    textureID = loadTexture(texturePath);
    setupMesh();
}

void Grass::setupMesh()
{
    // a single quad from (0,±0.5,0) to (1,±0.5,0)
    float quadVerts[] = {
        // positions        // texcoords
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f,
         0.0f, -0.5f, 0.0f,  0.0f, 1.0f,
         1.0f, -0.5f, 0.0f,  1.0f, 1.0f,

         0.0f,  0.5f, 0.0f,  0.0f, 0.0f,
         1.0f, -0.5f, 0.0f,  1.0f, 1.0f,
         1.0f,  0.5f, 0.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    // positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    // texcoords
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));

    glBindVertexArray(0);
}

void Grass::Draw(unsigned int shaderID,
                 const glm::mat4& view,
                 const glm::mat4& projection,
                 const glm::vec3& cameraPos)
{
    glUseProgram(shaderID);

    // set shared uniforms
    GLint locV = glGetUniformLocation(shaderID, "view");
    GLint locP = glGetUniformLocation(shaderID, "projection");
    GLint locCP= glGetUniformLocation(shaderID, "cameraPos");
    glUniformMatrix4fv(locV, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(locP, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(locCP,1, glm::value_ptr(cameraPos));

    // bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // pivot offset (center of quad)
    const glm::vec3 pivot(0.5f, 0.0f, 0.0f);

    glBindVertexArray(VAO);
    for (auto& pos : positions)
    {
        // compute rotation around Y so blade faces camera
        glm::vec3 dir = cameraPos - pos;
        dir.y = 0.0f;
        dir = glm::normalize(dir);
        float angle = glm::atan(dir.x, dir.z);

        // build per‑blade model
        glm::mat4 M(1.0f);
        M = glm::translate(M, pos);
        M = glm::translate(M, pivot);               // move pivot to origin
        M = glm::rotate(M, angle, glm::vec3(0,1,0));
        M = glm::translate(M, -pivot);              // move pivot back

        // upload & draw
        GLint locM = glGetUniformLocation(shaderID, "model");
        glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(M));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
}

unsigned int Grass::loadTexture(const char* path)
{
    unsigned int ID;
    glGenTextures(1, &ID);
    int w,h,n;
    unsigned char* data = stbi_load(path,&w,&h,&n,0);
    if (!data) {
        std::cerr<<"Failed to load grass texture "<<path<<"\n";
        return 0;
    }
    GLenum fmt = (n==4?GL_RGBA:(n==3?GL_RGB:GL_RED));
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    // clamp edges for alpha
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,fmt==GL_RGBA?GL_CLAMP_TO_EDGE:GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,fmt==GL_RGBA?GL_CLAMP_TO_EDGE:GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);
    return ID;
}
