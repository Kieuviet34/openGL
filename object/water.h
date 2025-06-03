#ifndef WATER_H
#define WATER_H

#include "../lib/glad.h"
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"

/**
 * Class Water:
 *  - Tạo 2 FBO: reflectionFBO (kèm RBO depth) và refractionFBO (kèm Depth Texture).
 *  - Load DuDv map + Normal map để tạo distortion và specular highlight.
 *  - Quad nằm tại y = waterHeight, kích thước 2*quadSize.
 *  - Dùng shader water.vs / water.fs để vẽ, với projective texturing (qua clipSpace→NDC).
 *  - Pha trộn reflection/refraction theo Fresnel Schlick, depth‐based tint, specular.
 */
class Water {
public:
    /**
     * @param dudvPath   Đường dẫn tới DuDv map (thường là PNG hoặc JPG).
     * @param normalPath Đường dẫn tới Normal map tương ứng.
     * @param width      Chiều rộng của texture reflection / refraction (FBO size).
     * @param height     Chiều cao của texture reflection / refraction.
     * @param waterH     Hàm lượng cao y (height) của mặt nước trong world space.
     * @param quadSize   Bán kính của quad (tức quad rộng 2*quadSize).
     */
    Water(const char* dudvPath,
          const char* normalPath,
          int         width,
          int         height,
          float       waterH,
          float       quadSize);

    ~Water();

    /// Trước khi render “Reflection Pass” (scene phía trên mặt nước qua mirrored camera).
    void BindReflectionFrameBuffer();

    /// Trước khi render “Refraction Pass” (scene phía dưới mặt nước).
    void BindRefractionFrameBuffer();

    /// Quay về render lên màn hình (default framebuffer).
    void UnbindFrameBuffer(int screenWidth, int screenHeight);

    /**
     * Vẽ mặt nước (đã có sẵn reflectionTexture, refractionTexture, refractionDepthTexture).
     * @param M           Model matrix cho quad (thường là translate(0, waterH, 0)).
     * @param V           View matrix (camera thật).
     * @param P           Projection matrix.
     * @param eyePoint    Vị trí camera (world space).
     * @param lightPos    Vị trí đèn (world space).
     * @param lightColor  Màu đèn (RGB).
     * @param dudvMove    Giá trị dịch chuyển DuDv (0→1) để duy trì ripples theo thời gian.
     * @param skyboxCubemap (Không bắt buộc) Texture cube map của skybox (nếu muốn hàm environment).  
     *                      Trong shader hiện tại bạn có uniform samplerCube nhưng chưa dùng, nên truyền vào nếu cần.
     */
    void Draw(const glm::mat4& M,
              const glm::mat4& V,
              const glm::mat4& P,
              const glm::vec3& eyePoint,
              const glm::vec3& lightPos,
              const glm::vec3& lightColor,
              float            dudvMove,
              GLuint           skyboxCubemap = 0);

    /// Lấy ra reflection texture ID.
    GLuint getReflectionTexture()   const { return reflectionTexture; }
    /// Lấy ra refraction texture ID.
    GLuint getRefractionTexture()   const { return refractionTexture; }
    /// Lấy ra depth texture của refraction (dùng trong water.fs để tint nước theo depth).
    GLuint getDepthTexture()        const { return refractionDepthTexture; }

private:
    /// Khởi tạo 1 FBO:
    /// - Nếu isReflection == true: tạo color texture + RBO depth.
    /// - Nếu isReflection == false: tạo color texture + depth texture.
    void InitializeFrameBuffer(GLuint& fbo,
                               GLuint& colorTexture,
                               GLuint& depthAttachment,
                               bool    isReflection);

    /// Load 1 texture 2D (DuDv or Normal). Kết quả lưu vào textureID.
    void LoadTexture(const char* path, GLuint& textureID);

    /// Tạo quad (6 vertices) với layout [pos.xyz | uv.xy].
    void CreateWaterQuad();

    // FBOs / texture attachments
    GLuint reflectionFBO;
    GLuint reflectionTexture;
    GLuint reflectionDepthBuffer;    // RBO nếu isReflection

    GLuint refractionFBO;
    GLuint refractionTexture;
    GLuint refractionDepthTexture;   // depth texture nếu !isReflection

    // DuDv + Normal
    GLuint dudvTexture;
    GLuint normalMapTexture;

    // Quad VAO/VBO
    GLuint waterVAO;
    GLuint waterVBO;

    // Shader
    Shader waterShader;

    int    reflectionWidth;
    int    reflectionHeight;
    float  waterHeight;
    float  quadSize;
};

#endif // WATER_H
