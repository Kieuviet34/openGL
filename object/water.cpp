#include "water.h"
#include "../lib/stb_image.h"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

Water::Water(const char* dudvPath,
             const char* normalPath,
             int         width,
             int         height,
             float       waterH,
             float       quadSize_)
    : reflectionWidth(width),
      reflectionHeight(height),
      waterHeight(waterH),
      quadSize(quadSize_),
      waterShader("shaders/water.vs", "shaders/water.fs")
{
    // 1) Tạo Reflection FBO (có RBO làm depth)
    InitializeFrameBuffer(reflectionFBO, reflectionTexture, reflectionDepthBuffer, true);

    // 2) Tạo Refraction FBO (có depth texture)
    InitializeFrameBuffer(refractionFBO, refractionTexture, refractionDepthTexture, false);

    // 3) Load DuDv map và Normal map
    LoadTexture(dudvPath,    dudvTexture);
    LoadTexture(normalPath,  normalMapTexture);

    // 4) Tạo quad (mặt phẳng) ở y = 0 (model matrix sẽ translate lên y = waterHeight)
    CreateWaterQuad();

    // 5) Thiết lập những giá trị constant trong shader (chỉ gọi 1 lần)
    waterShader.use();
    waterShader.setInt("texReflect",     0);
    waterShader.setInt("texRefract",     1);
    waterShader.setInt("texDudv",        2);
    waterShader.setInt("texNormal",      3);
    waterShader.setInt("texDepthRefract",1); // refractionDepthTexture dùng chung slot với texRefract (đọc r-channel)
    waterShader.setInt("texSkybox",      4);
}

Water::~Water() {
    // Xóa FBO, textures, buffers
    glDeleteFramebuffers(1, &reflectionFBO);
    glDeleteFramebuffers(1, &refractionFBO);
    glDeleteTextures(1,     &reflectionTexture);
    glDeleteTextures(1,     &refractionTexture);
    glDeleteRenderbuffers(1,&reflectionDepthBuffer);
    glDeleteTextures(1,     &refractionDepthTexture);
    glDeleteTextures(1,     &dudvTexture);
    glDeleteTextures(1,     &normalMapTexture);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1,      &waterVBO);
}

void Water::InitializeFrameBuffer(GLuint& fbo,
                                  GLuint& colorTexture,
                                  GLuint& depthAttachment,
                                  bool    isReflection)
{
    // 1. Sinh FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // 2. Tạo color attachment (texture RGB)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, reflectionWidth, reflectionHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    if (isReflection) {
        // 3a) Nếu là Reflection: tạo Renderbuffer làm depth (RBO)
        glGenRenderbuffers(1, &depthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, reflectionWidth, reflectionHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthAttachment);
    } else {
        // 3b) Nếu là Refraction: tạo Depth Texture
        glGenTextures(1, &depthAttachment);
        glBindTexture(GL_TEXTURE_2D, depthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, reflectionWidth, reflectionHeight, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Swizzle để đọc depth từ kênh R
        GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttachment, 0);
    }

    // 4. Kiểm tra completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Water FBO is not complete!" << std::endl;
    }

    // 5. Unbind
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Water::LoadTexture(const char* path, GLuint& textureID) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }
    GLenum format = (n == 4 ? GL_RGBA : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set wrap & filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

void Water::CreateWaterQuad() {
    // Một quad nằm trên y=0, toạ độ x/z ∈ [-quadSize, +quadSize], UV ∈ [0,1]
    float s = quadSize;
    float vertices[] = {
        // x,    y,    z,    u, v
        -s,  0.0f, -s,    0.0f, 0.0f,
         s,  0.0f, -s,    1.0f, 0.0f,
        -s,  0.0f,  s,    0.0f, 1.0f,
         s,  0.0f, -s,    1.0f, 0.0f,
         s,  0.0f,  s,    1.0f, 1.0f,
        -s,  0.0f,  s,    0.0f, 1.0f
    };

    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1,      &waterVBO);

    glBindVertexArray(waterVAO);
      glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

      // layout(location = 0) = vec3 position
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);

      // layout(location = 1) = vec2 uv
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Water::BindReflectionFrameBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glViewport(0, 0, reflectionWidth, reflectionHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Water::BindRefractionFrameBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, refractionFBO);
    glViewport(0, 0, reflectionWidth, reflectionHeight);
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
                 float            dudvMove,
                 GLuint           skyboxCubemap)
{
    // 1) Sử dụng shader
    waterShader.use();

    // 2) Truyền các uniform matrix + ánh sáng + camera
    waterShader.setMat4("M", M);
    waterShader.setMat4("V", V);
    waterShader.setMat4("P", P);
    waterShader.setVec3("eyePoint",     eyePoint);
    waterShader.setVec3("lightPos",     lightPos);
    waterShader.setVec3("lightColor",   lightColor);
    waterShader.setFloat("dudvMove",    dudvMove);

    // 3) Nếu có skyboxCubemap, bind nó vào slot 4
    if (skyboxCubemap != 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
    }

    // 4) Bind các texture vào đúng đơn vị đã set uniform:
    //   texReflect  → GL_TEXTURE0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);

    //   texRefract  → GL_TEXTURE1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, refractionTexture);

    //   texDudv     → GL_TEXTURE2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, dudvTexture);

    //   texNormal   → GL_TEXTURE3
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, normalMapTexture);

    //   texDepthRefract (đọc r-channel từ refractionDepthTexture) → GL_TEXTURE1 (chồng chung với texRefract)
    //   Trong shader bạn đã dùng texture(texDepthRefract, ndc).r; nên chỉ cần active lại GL_TEXTURE1 
    //   (nó vẫn đang lưu refractionTexture) vì việc swizzle đã được set ở InitializeFrameBuffer.
    //   Nếu bạn muốn tách riêng slot, có thể set texDepthRefract = 5 và bind GL_TEXTURE5 → refractionDepthTexture.

    // 5) Vẽ quad (6 điểm)
    glBindVertexArray(waterVAO);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLES, 0, 6);
      glDisable(GL_BLEND);
    glBindVertexArray(0);

    // 6) Sau cùng, nếu đã bind skybox, bạn có thể unbind nếu muốn:
    if (skyboxCubemap != 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
}
