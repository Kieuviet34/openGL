#include "water.h"
#include "../lib/stb_image.h"
#include <iostream>
#include <cmath>

Water::Water(const char* dudvPath,
             const char* normalPath,
             int width, int height,
             float waterH)
    : reflectionWidth(width)
    , reflectionHeight(height)
    , waterHeight(waterH)
    , waterShader("shaders/water.vs", "shaders/water.fs") 
{
    // reflection FBO (depth RBO)
    InitializeFrameBuffer(reflectionFBO, reflectionTexture, reflectionDepthBuffer, true);
    // refraction FBO (depth texture)
    InitializeFrameBuffer(refractionFBO, refractionTexture, refractionDepthTexture, false);
    // load textures
    LoadTexture(dudvPath, dudvTexture);
    LoadTexture(normalPath, normalMapTexture);
    // quad
    CreateWaterQuad();
}

Water::~Water() {
    glDeleteFramebuffers(1, &reflectionFBO);
    glDeleteFramebuffers(1, &refractionFBO);
    glDeleteTextures(1, &reflectionTexture);
    glDeleteTextures(1, &refractionTexture);
    glDeleteRenderbuffers(1, &reflectionDepthBuffer);
    glDeleteTextures(1, &refractionDepthTexture);
    glDeleteTextures(1, &dudvTexture);
    glDeleteTextures(1, &normalMapTexture);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
}

void Water::BindReflectionFrameBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Water::BindRefractionFrameBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, refractionFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Water::UnbindFrameBuffer(int screenWidth, int screenHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth, screenHeight);
}

void Water::Draw(const glm::mat4& M,
                 const glm::mat4& V,
                 const glm::mat4& P,
                 const glm::vec3& eyePoint,
                 const glm::vec3& lightPos,
                 const glm::vec3& lightColor,
                 float dudvMove,
                 GLuint skyboxCubemap)
{
    waterShader.use();
    waterShader.setMat4("M", M);
    waterShader.setMat4("V", V);
    waterShader.setMat4("P", P);
    waterShader.setVec3("eyePoint", eyePoint);
    waterShader.setVec3("lightPos", lightPos);
    waterShader.setVec3("lightColor", lightColor);
    waterShader.setFloat("dudvMove", dudvMove);
    waterShader.setInt("texSkybox", 4);

    // set samplers
    waterShader.setInt("texReflect", 0);
    waterShader.setInt("texRefract", 1);
    waterShader.setInt("texDudv",    2);
    waterShader.setInt("texNormal",  3);
    waterShader.setInt("texDepthRefr",1); // uses refraction depth

    // bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, refractionTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, dudvTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, normalMapTexture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);

    glBindVertexArray(waterVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Water::InitializeFrameBuffer(GLuint& fbo,
                                  GLuint& texture,
                                  GLuint& depthAttachment,
                                  bool isReflection)
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // color
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, reflectionWidth, reflectionHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (isReflection) {
        glGenRenderbuffers(1, &depthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, reflectionWidth, reflectionHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthAttachment);
    } else {
        glGenTextures(1, &depthAttachment);
        glBindTexture(GL_TEXTURE_2D, depthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, reflectionWidth, reflectionHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttachment, 0);
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "FBO not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Water::CreateWaterQuad() {
    float size = 50.0f; // unit quad
    float vertices[] = {
        -size, 0, -size,  0,0,
         size, 0, -size,  1,0,
        -size, 0,  size,  0,1,
         size, 0, -size,  1,0,
         size, 0,  size,  1,1,
        -size, 0,  size,  0,1
    };
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Water::LoadTexture(const char* path, GLuint& textureID) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    int w,h,n;
    unsigned char* data = stbi_load(path,&w,&h,&n,0);
    if (!data) { std::cerr<<"fail load "<<path<<std::endl; return; }
    GLenum fmt = (n==4?GL_RGBA:GL_RGB);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);
}
