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

glm::vec3 treePos    = glm::vec3(0.0f, 0.0f, 0.0f);
int       grassCount =  5;      
float     grassArea  = 25.0f;   
std::vector<glm::vec3> vegetation;

void processInput(GLFWwindow * window, bool& showAxes);
void framebuffer_size_callback(GLFWwindow * window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow * window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
unsigned int loadTexture(const char *path);
unsigned int loadCubemap(std::vector<std::string> faces);
void regenerateGrass();

Camera camera(glm::vec3(0.6f, 8.8f, 12.0f));
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


    enum ViewPreset { DEFAULT, TOP, FRONT, SIDE };
    ViewPreset currentView = DEFAULT;

    // Shaders
    Shader shader("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader grassShader("shaders/grassVertex.gsls", "shaders/grassFragment.gsls");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
    Shader nmShader("shaders/normal_mapping.vs", "shaders/normal_mapping.fs");
    Shader modelShader("shaders/model_loading.vs",
                          "shaders/model_loading.fs");
    Shader litShader("shaders/lit.vs", "shaders/lit.fs");

    // Scene objects
    Model TreeModel("assets/model/tree/Tree.obj");
    Model testModel("assets/model/plane/piper_pa18.obj");
    Axes axes;

    Skybox skybox({
        "assets/skybox/right.jpg", "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",   "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg", "assets/skybox/back.jpg"
    });
    LightSphere lightViz(16,16, glm::vec3(1.0f,.8f,0.2f));
    Ground ground("assets/texture/brickwall.jpg");
    regenerateGrass();
    Grass grass("assets/texture/grass.png", vegetation);

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

    grassShader.use();
    grassShader.setInt("texture1", 0);
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window, showAxes);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // common matrices
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)WIDTH/HEIGHT, 0.1f, 1000.0f);
        glDisable(GL_CULL_FACE);
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

        
        litShader.use();
        litShader.setMat4("projection", projection);
        litShader.setMat4("view",       view);
        litShader.setVec3("lightPos",   lightPos);
        litShader.setVec3("viewPos",    camera.Position);
        litShader.setVec3("lightColor", glm::vec3(1.0f));
        // 2) RENDER MODEL (USED TO BE RENDER CUBE)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 M = glm::translate(glm::mat4(1.0f), treePos);
        litShader.setMat4("model", M);
        testModel.Draw(litShader);
        glDisable(GL_BLEND);
        
        // 3) RENDER GRASS
        glEnable(GL_BLEND);
        grassShader.use();
        grassShader.setMat4("projection", projection);
        grassShader.setMat4("view",       view);
        grassShader.setMat4("model",      glm::mat4(1.0f));
        grass.Draw(grassShader.ID, view, projection,camera.Position);
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
        glEnable(GL_CULL_FACE);
        // ImGui window
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Light & Settings");

        
        //tree pos
        ImGui::SliderFloat3("Tree Position", glm::value_ptr(treePos),
                    -20.0f, 20.0f);

        // — Grass density —  
        if (ImGui::SliderInt("Grass Count", &grassCount, 0, 100))
        {
            regenerateGrass();
            grass = Grass("assets/texture/grass.png", vegetation);
        }
        ImGui::Text("Move light");
        ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.1f);
        ImGui::Checkbox("Show Axes", &showAxes);
        ImGui::Separator();
        ImGui::Text("View Presets:");
        if (ImGui::Button("Default View")) {
            camera.Position = glm::vec3(0.6f, 8.8f, 12.0f);
            camera.Yaw = -90.0f;
            camera.Pitch = -40.0f;
            camera.SetViewPreset(glm::vec3(0.6f, 8.8f, 12.0f), -90.0f, -20.0f);
        }
        if (ImGui::Button("Top View")) {
            camera.Position = glm::vec3(0.0f, 250.0f, 0.1f);
            camera.Yaw = -90.0f;
            camera.Pitch = -89.9f;
            camera.SetViewPreset(glm::vec3(0.0f, 250.0f, 0.1f), -90.0f, -89.9f);
        }
        if (ImGui::Button("Front View")) {
            camera.Position = glm::vec3(0.0f, 5.0f, 150.0f);
            camera.Yaw = -90.0f;
            camera.Pitch = 0.0f;
            camera.SetViewPreset(glm::vec3(0.0f, 5.0f, 150.0f), -90.0f, 0.0f);
        }
        if (ImGui::Button("Side View")) {
            camera.Position = glm::vec3(150.0f, 5.0f, 0.0f);
            camera.Yaw = 180.0f;
            camera.Pitch = 0.0f;
            camera.SetViewPreset(glm::vec3(150.0f, 5.0f, 0.0f), 180.0f, 0.0f);
        }//fps
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
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
    float speedMultiplier = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speedMultiplier = 8.0f; 
    }

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
        camera.ProcessKeyboard(FORWARD, deltaTime * speedMultiplier);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime * speedMultiplier);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime * speedMultiplier);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime * speedMultiplier);
}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            camera.LeftMousePressed = true;
            firstMouse = true;        // <— ADD THIS
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
void regenerateGrass()
{
    vegetation.clear();

    // We’ll use grassCount as “blades per row”,
    // so total blades = grassCount * grassCount
    int bladesPerRow = grassCount;
    vegetation.reserve(bladesPerRow * bladesPerRow);

    for (int i = 0; i < bladesPerRow; ++i)
    {
        for (int j = 0; j < bladesPerRow; ++j)
        {
            // normalized [0..1]
            float u = (i + ((float)rand() / RAND_MAX)) / bladesPerRow;
            float v = (j + ((float)rand() / RAND_MAX)) / bladesPerRow;

            // remap to [–0.5..+0.5] then scale by grassArea
            float x = (u - 0.5f) * grassArea;
            float z = (v - 0.5f) * grassArea;

            vegetation.emplace_back(x, 0.0f, z);
        }
    }
}
