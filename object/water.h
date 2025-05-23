#ifndef WATER_H
#define WATER_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"

class Water {
public:
    Water(const char* dudvPath,
          const char* normalPath,
          int width, int height,
          float waterH);
    ~Water();

    // Render into reflection / refraction
    void BindReflectionFrameBuffer();
    void BindRefractionFrameBuffer();
    void UnbindFrameBuffer(int screenWidth, int screenHeight);

    // Draw final water surface
    void Draw(const glm::mat4& M,
              const glm::mat4& V,
              const glm::mat4& P,
              const glm::vec3& eyePoint,
              const glm::vec3& lightPos,
              const glm::vec3& lightColor,
              float dudvMove,
              GLuint skyboxCubemap);

private:
    void InitializeFrameBuffer(GLuint& fbo,
                               GLuint& texture,
                               GLuint& depthAttachment,
                               bool isReflection);
    void LoadTexture(const char* path, GLuint& textureID);
    void CreateWaterQuad();

    // FBOs / attachments
    GLuint reflectionFBO, refractionFBO;
    GLuint reflectionTexture, refractionTexture;
    GLuint reflectionDepthBuffer;
    GLuint refractionDepthTexture;

    // Water rendering
    GLuint waterVAO, waterVBO;
    GLuint dudvTexture, normalMapTexture;

    Shader waterShader;
    int    reflectionWidth, reflectionHeight;
    float  waterHeight;
};

#endif // WATER_H
