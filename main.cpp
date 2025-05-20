#include "lib/glad.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include "ultis/shaderReader.h"
#include "ultis/camera.h"
#include "object/axes.h"
#include "object/cube.h"
#include "object/grass.h"
#include "object/ground.h"
#include "object/light.h"
#include "ultis/model.h"
#include "object/skybox.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>

const int WIDTH = 1920;
const int HEIGHT = 1080;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

void processInput(GLFWwindow * window, bool& showAxes);
void framebuffer_size_callback(GLFWwindow * window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow * window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
unsigned int loadTexture(const char *path);
unsigned int loadCubemap(std::vector<std::string> faces);

Camera camera(glm::vec3(0.6f, .8f, 3.0f));
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

int main(){
    // Initialize GLFW
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Map generated tool", NULL, NULL);
    if (!window) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (mode->width - WIDTH) / 2, (mode->height - HEIGHT) / 2);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; return -1; }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Shaders
    Shader shader("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader grassShader("shaders/grassVertex.gsls", "shaders/grassFragment.gsls");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
    Shader nmShader("shaders/normal_mapping.vs", "shaders/normal_mapping.fs");

    // Scene objects
    Axes axes;
    Cube cube;
    Skybox skybox({
        "assets/skybox/right.jpg", "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",   "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg", "assets/skybox/back.jpg"
    });
    LightSphere lightViz(16,16, glm::vec3(1.0f,.8f,0.2f));
    Ground ground("assets/texture/brickwall.jpg");
    Grass grass("assets/texture/grass.png", {
        {-1.5f,0.0f,-0.48f}, {1.5f,0.0f,0.51f}, {0.0f,0.0f,0.7f},
        {-0.3f,0.0f,-2.3f}, {0.5f,0.0f,-0.6f}
    });

    bool showAxes = false;
    float cubePosX=0, cubePosY=0, cubePosZ=0;

    // normal mapping textures
    unsigned int diffuseMap = loadTexture("assets/texture/brickwall.jpg");
    unsigned int normalMap  = loadTexture("assets/texture/brickwall_normal.jpg");

    // light
    glm::vec3 lightPos(0.0f, 20.0f, 2.0f);

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window, showAxes);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // common matrices
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)WIDTH/HEIGHT, 0.1f, 100.0f);

        lightViz.Draw(projection, view, lightPos);
        // 1) NORMAL MAPPED & LIT FLOOR
        nmShader.use();
        nmShader.setMat4("projection", projection);
        nmShader.setMat4("view", view);
        nmShader.setMat4("model", glm::mat4(1.0f));
        nmShader.setVec3("lightPos", lightPos);
        nmShader.setVec3("viewPos", camera.Position);
        nmShader.setInt("diffuseMap", 0);
        nmShader.setInt("normalMap",  1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMap);
        glBindVertexArray(ground.getVAO());
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // 2) RENDER CUBE
        shader.use();
        shader.setMat4("model", glm::translate(glm::mat4(1.0f), {cubePosX,cubePosY,cubePosZ}));
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        cube.render();

        // 3) RENDER GRASS
        glEnable(GL_BLEND);
        grassShader.use();
        grassShader.setMat4("view", view);
        grassShader.setMat4("projection", projection);
        grass.Draw(grassShader.ID);
        glDisable(GL_BLEND);

        if(showAxes) axes.render();

        // 4) SKYBOX
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
        skyboxShader.setMat4("projection", projection);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getTextureID());
        skybox.render();
        glDepthFunc(GL_LESS);
        // ImGui window
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Light & Settings");
        ImGui::Text("Move light");
        ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.1f);
        ImGui::Checkbox("Show Axes", &showAxes);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!camera.LeftMousePressed) return; 

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
void processInput(GLFWwindow* window, bool& showAxes) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window, true);
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        showAxes = true;
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_RELEASE) {
        showAxes = false;
    }
     if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            camera.LeftMousePressed = true;
        } else if (action == GLFW_RELEASE) {
            camera.LeftMousePressed = false;
        }
    }
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
