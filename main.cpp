// test.cpp
#include "lib/glad.h"
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "ultis/shaderReader.h"
#include "ultis/camera.h"
#include "ultis/model.h"
#include "terrain/terrain.h"
#include "object/skybox.h"
#include "object/water.h"
#include "object/light.h"
#include "object/spotLight.hpp"
#include "object/sphere.hpp"
#include "terrain/lodterrain.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <iostream>


//global values
static const int SCR_WIDTH = 1280;
static const int SCR_HEIGHT = 720;
const unsigned int SHADOW_WIDTH  = 720;
const unsigned int SHADOW_HEIGHT = 720;
float WATER_HEIGHT = -110.5f;
float fogStart = 350.f;
float fogEnd = 1250.f;
float terrainScale = 64.0f;
float worldSize = 700.0f;
static float dayNightCycleSeconds = 60.0f; 
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;
float dudvMove = 0.3f;
float ambientStrength = 0.4f;
static bool showDebug = false;
bool spotlightEnabled = false;
bool spotlightOnly = false;
static const int NUM_TREES = 25;
static const int NUM_LAMPS = 5;



std::vector<glm::vec2> lampPositions;
std::vector<glm::vec2> treePositions;
std::vector<SpotLight> lampLights;
std::vector<glm::vec3> bulbWorldPositions;


// Framebuffer và texture để lưu depth từ ánh sáng
GLuint depthMapFBO = 0;
GLuint depthMap    = 0;
glm::vec3 fogColor = glm::vec3(0.7f, 0.8f, 1.0f);
// setup light
glm::vec3 lightPos = glm::vec3(0.0f, 150.0f, 0.0f);
glm::vec3 lightColor = glm::vec3(1.0f, 0.92f, 0.85f);

enum ViewPreset
{
    DEFAULT,
    TOP,
    FRONT,
    SIDE
};
ViewPreset currentView = DEFAULT;

// Callbacks
void framebuffer_size_callback(GLFWwindow *, int, int);
void mouse_callback(GLFWwindow *, double, double);
void scroll_callback(GLFWwindow *, double, double);
void mouse_button_callback(GLFWwindow *, int, int, int);
void processInput(GLFWwindow *);



Camera camera(glm::vec3(10.0f, 65.0f, 35.0f));




int main()
{
    // — GLFW + GLAD init —
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Terrain Generator", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // — ImGui init —
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    // — Shaders —
    Shader terrainShader("shaders/terrain.vs", "shaders/terrain.fs");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
    Shader litShader("shaders/lit.vs", "shaders/lit.fs");
    Shader waterShader("shaders/water.vs", "shaders/water.fs");
    Shader depthShader("shaders/shadow_depth.vs", "shaders/shadow_depth.fs");
    Shader sphereShader("shaders/lightSphere.vs", "shaders/lightSphere.fs");
    Shader treeShader = litShader;
    Shader lampShader = litShader;

    LightSphere lightViz(16, 16, lightColor);
    Sphere lightSphere;
    lightSphere.build(32, 16); 

    LodTerrain lodTerrain(
        /*tileSize=*/128,
        /*lodLevels=*/4,
        /*scale=*/worldSize,
        /*heightScale=*/340.3f,
        /*smoothness=*/1.f,
        "assets/texture/ground/textures/aerial_rocks_04_diff_1k.jpg",
        "assets/texture/ground/textures/aerial_rocks_04_nor_gl_1k.jpg");

    Skybox skybox({"assets/skybox/right.jpg", "assets/skybox/left.jpg",
                   "assets/skybox/top.jpg", "assets/skybox/bottom.jpg",
                   "assets/skybox/front.jpg", "assets/skybox/back.jpg"});
    Water water(
        "assets/texture/waterDudv.png",
        "assets/texture/waterNormal.png",
        SCR_WIDTH * 1.5, SCR_HEIGHT * 1.5,
        WATER_HEIGHT,
        worldSize);
    Model tree("assets/model/lowpolytree/Tree3_1.obj");
    Model lamp("assets/model/lamp/LAMP_OBJ.obj");
    
    
    
    float treeBaseOffset = 0.0f;
    {
        bool firstVertex = true;
        for (const auto& mesh : tree.meshes) {
            for (const auto& v : mesh.vertices) {
                float vy = v.Position.y;
                if (firstVertex) {
                    treeBaseOffset = vy;
                    firstVertex = false;
                } else {
                    treeBaseOffset = std::min(treeBaseOffset, vy);
                }
            }
        } 
        std::cout << "treeBaseOffset = " << treeBaseOffset << std::endl;
    }
    //just calculate max distance from surface to highest point of lamp object (from spotligh placing)
    {
        float lampTopOffset = -std::numeric_limits<float>::infinity();
        for (const auto& mesh : lamp.meshes) {
            for (const auto& v : mesh.vertices) {
                lampTopOffset = std::max(lampTopOffset, v.Position.y);
            }
        }
        std::cout << "lampTopOffset = " << lampTopOffset << std::endl;
    }
    {
        std::mt19937 gen((unsigned int)glfwGetTime());
        std::uniform_real_distribution<float> distXZ(-worldSize * 0.5f, worldSize * 0.5f);

        treePositions.clear();
        while ((int)treePositions.size() < NUM_TREES) {
            float rx = distXZ(gen);
            float rz = distXZ(gen);
            float ry = lodTerrain.getHeightAt(rx, rz);
            if (ry > WATER_HEIGHT) {
                treePositions.emplace_back(rx, rz);
            }
        }
    }

    {
        std::mt19937 gen((unsigned int)glfwGetTime() + 121);   
        std::uniform_real_distribution<float> distXZ(-worldSize / 2.3, worldSize / 2.3);

        lampPositions.clear();
        while ((int)lampPositions.size() < NUM_LAMPS) {
            float rx = distXZ(gen);
            float rz = distXZ(gen);
            float ry = lodTerrain.getHeightAt(rx, rz);
            if (ry > WATER_HEIGHT) {
                lampPositions.emplace_back(rx, rz);
            }
        }
    }



    //create depth map for shadow
    {
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                    SHADOW_WIDTH, SHADOW_HEIGHT, 
                    0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        // Filter ± wrap
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // 2) Tạo framebuffer depthMapFBO và attach depthMap làm DEPTH_ATTACHMENT
        glGenFramebuffers(1, &depthMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        // Chỉ cần depth, không cần color
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR::SHADOW_MAP:: Framebuffer not complete!" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }


    //calc heigh of 2 bulb (model)
    float lampModelTopY = -FLT_MAX;
    for (auto const& mesh : lamp.meshes) {
        for (auto const& v : mesh.vertices) {
            lampModelTopY = max(lampModelTopY, v.Position.y);
        }
    }
    // pick a small horizontal offset to spread the two bulbs apart
    float armOffsetX = 1.8f;

    std::vector<glm::vec4> bulbLocals = {
    // in LOCAL lamp‐space, use the real model top Y
        glm::vec4(0.0f, lampModelTopY - 1.7f,  armOffsetX, 1.0f),  // left
        glm::vec4(0.0f, lampModelTopY - 1.7f,  -armOffsetX, 1.0f),  // right
    };
    // — Render loop —
    while (!glfwWindowShouldClose(window))
    {
        // timing
        float current = (float)glfwGetTime();
        deltaTime = current - lastFrame;
        lastFrame = current;
        processInput(window);

        dudvMove += deltaTime * 0.02f;
        dudvMove = fmod(dudvMove, .2f);


        //spotlight 
        bulbWorldPositions.clear();
        lampLights.clear();

        for (auto const & pos : lampPositions) {
            float lx = pos.x, lz = pos.y;
            float ly = lodTerrain.getHeightAt(lx, lz);

            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(lx, ly, lz))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));

            

            for (int i = 0; i < 2; ++i) {
                glm::vec3 bulbWorld = glm::vec3(M * bulbLocals[i]);
                bulbWorldPositions.push_back(bulbWorld);

                glm::vec3 bulbDir = glm::normalize(glm::vec3(0, -1, (i==0?+1.f:-1.f)));

                SpotLight light(glm::vec3(1,0.85f,0.6f), 45.5f, 60.5f);
                light.Position    = bulbWorld;
                light.Direction   = bulbDir;
                light.CutOff      = glm::cos(glm::radians(45.0f));
                light.OuterCutOff = glm::cos(glm::radians(60.5f));
                light.Constant    = 1.0f;
                light.Linear      = 0.02f;
                light.Quadratic   = 0.005f;
                lampLights.push_back(light);
            }
        }



        // ———————— Sun‐Cycle Update ————————
        const float DAY_LENGTH = dayNightCycleSeconds;
        const float TWO_PI = glm::two_pi<float>();
        
        // Calculate sun angle (0 to 2π) based on time
        float timeOfDay = fmod(glfwGetTime(), DAY_LENGTH) / DAY_LENGTH;
        float sunAngle = timeOfDay * TWO_PI;
        
        // Place sun on circle around X axis
        const float SUN_RADIUS = 600.0f;
        lightPos = glm::vec3(
            0.0f,                        
            SUN_RADIUS * sin(sunAngle), 
            SUN_RADIUS * -cos(sunAngle) 
        );
        
        float dayFactor = glm::clamp(lightPos.y / SUN_RADIUS, 0.0f, 1.0f);
        
        const float NIGHT_AMB = 0.05f;
        const float DAY_AMB = 0.4f;
        ambientStrength = glm::mix(NIGHT_AMB, DAY_AMB, dayFactor);
        
        // Sun colors
        const glm::vec3 NOON_COLOR(1.0f, 0.92f, 0.85f);
        const glm::vec3 NIGHT_COLOR(0.1f, 0.1f, 0.15f);
        const glm::vec3 SUNRISE_COLOR(1.0f, 0.6f, 0.4f);
        
        // Smooth sunrise/sunset transition
        float transition = glm::smoothstep(0.0f, 0.2f, dayFactor) - 
                          glm::smoothstep(0.8f, 1.0f, dayFactor);
        
        lightColor = glm::mix(
            glm::mix(NIGHT_COLOR, NOON_COLOR, dayFactor),
            SUNRISE_COLOR,
            transition
        );
        
        // Scale sun visualization
        float sphereScale = glm::mix(0.5f, 3.0f, dayFactor);




        // 1) Tính ma trận chiếu ánh sáng (lightSpaceMatrix)
        glm::mat4 lightSpaceMatrix;
        if (lightPos.y > 0.0f) {
            glm::vec3 terrainCenter = glm::vec3(0.0f);

            // Hướng ánh sáng gốc
            glm::vec3 dir = glm::normalize(lightPos - terrainCenter);
            // Chỉ giữ thành phần y ≥ 0 để frustum không bị ngược
            if (dir.y < 0.0f) dir.y = fabs(dir.y);
            dir = glm::normalize(dir);

            float dist = worldSize * 0.75f;
            glm::vec3 lightEye  = terrainCenter + dir * dist;
            glm::vec3 lightTarget = terrainCenter;
            glm::vec3 up = glm::vec3(0,1,0);

            glm::mat4 lightView = glm::lookAt(lightEye, lightTarget, up);
            float near_plane = 1.0f, far_plane = 2000.0f;
            float orthoSize = worldSize * 0.5f;
            glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize,
                                                -orthoSize, orthoSize,
                                                    near_plane, far_plane);
            lightSpaceMatrix = lightProjection * lightView;
        }



        // common matrices
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = glm::perspective(
            glm::radians(camera.Zoom),
            float(SCR_WIDTH) / SCR_HEIGHT,
            0.1f, 10000.0f);
        lightViz.Draw(proj, view, lightPos, sphereScale);


        // --- DEPTH PASS: chỉ khi lightPos.y > 0 ---
        
        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        //  Vẽ terrain vào shadow map
        glm::mat4 modelTerrain = glm::mat4(1.0f);
        depthShader.setMat4("model", modelTerrain);
        lodTerrain.Draw(camera.Position);

        //  Vẽ cây vào shadow map
        for (auto& pos : treePositions) {
            float wx = pos.x, wz = pos.y;
            float wy = lodTerrain.getHeightAt(wx, wz);
            glm::mat4 treeModel = glm::translate(glm::mat4(1.0f),
                                                glm::vec3(wx, wy - treeBaseOffset, wz));
            treeModel = glm::scale(treeModel, glm::vec3(1.5f));
            depthShader.setMat4("model", treeModel);
            tree.Draw(depthShader);
        }
        for (const glm::vec2& pos : lampPositions) {
            float lx = pos.x;
            float lz = pos.y;
            float ly = lodTerrain.getHeightAt(lx, lz);

            // translate so the lamp sits on the terrain
            glm::mat4 lampM = glm::translate(glm::mat4(1.0f),
                                                glm::vec3(lx, ly, lz));

            lampM = glm::scale(lampM, glm::vec3(1.5f));
            lampShader.setMat4("model", lampM);
            lamp.Draw(lampShader);
        }


        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        

        //
        // 1) REFLECTION PASS
        //
        water.BindReflectionFrameBuffer();
        glEnable(GL_CLIP_DISTANCE0);
        // flip camera over water
        float d = 2.0f * (camera.Position.y - WATER_HEIGHT);
        camera.Position.y -= d;
        camera.Pitch = -camera.Pitch;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::vec4 clipPlaneR = glm::vec4(0, 1, 0, -WATER_HEIGHT + 0.8);

        // 1a) terrain
        // — bind terrain shader —
        terrainShader.use();
        

        terrainShader.setInt("numSpotLights", (int)lampLights.size());
        for(int i=0; i<lampLights.size(); ++i){
            lampLights[i].ApplyToShader(terrainShader, "spotLights[" + std::to_string(i) + "]");
        }
        // textures
        terrainShader.setInt("albedoMap",   0);
        terrainShader.setInt("normalMap",   1);
        terrainShader.setInt("shadowMap", 5);
        terrainShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        // lighting
        terrainShader.setVec3("lightDir", glm::normalize(-lightPos));
        terrainShader.setVec3("lightColor", lightColor);
        terrainShader.setFloat("ambientStrength", ambientStrength);
        terrainShader.setVec3("viewPos",    camera.Position);
        terrainShader.setFloat("dayFactor", dayFactor);
        terrainShader.setBool("shadowsEnabled", lightPos.y > 0.0f);
        // fog
        terrainShader.setFloat("fogStart", fogStart);
        terrainShader.setFloat("fogEnd",   fogEnd);
        terrainShader.setVec3 ("fogColor", fogColor);

        // water tint
        terrainShader.setFloat("waterHeight", WATER_HEIGHT);
        terrainShader.setFloat("maxDepth",     25.0f);
        terrainShader.setVec3 ("shallowColor", glm::vec3(0.0f,0.25f,0.4f));
        terrainShader.setVec3 ("deepColor",    glm::vec3(0.0f,0.05f,0.2f));

        // transforms + clip
        glm::mat4 model = glm::mat4(1.0f);
        terrainShader.setMat4("model",      model);
        terrainShader.setMat4("view",       view);
        terrainShader.setMat4("projection", proj);
        terrainShader.setVec4("clipPlane",  clipPlaneR);

        // tiling scale
        terrainShader.setFloat("worldScale", worldSize);

        // draw
        lodTerrain.Draw(camera.Position);


        treeShader.use();
        treeShader.setMat4("view",       view);
        treeShader.setMat4("projection", proj);
        treeShader.setVec3("lightPos",   lightPos);
        treeShader.setVec3("viewPos",    camera.Position);
        treeShader.setVec3("lightColor", lightColor);
        treeShader.setFloat("fogStart", fogStart);
        treeShader.setFloat("fogEnd",   fogEnd);
        treeShader.setVec3("fogColor",  fogColor);
        treeShader.setInt("shadowMap", 5);
        treeShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        for (const glm::vec2& pos : treePositions) {
            float wx = pos.x;
            float wz = pos.y;
            float wy = lodTerrain.getHeightAt(wx, wz);

            glm::mat4 treeModel = glm::mat4(1.0f);
            // Dịch origin của cây đến y = wy – treeBaseOffset
            treeModel = glm::translate(treeModel,
                                       glm::vec3(wx, wy - treeBaseOffset, wz));
            // Scale cây (nếu muốn)
            treeModel = glm::scale(treeModel, glm::vec3(1.5f));

            treeShader.setMat4("model", treeModel);
            tree.Draw(treeShader);
        }


        lampShader.use();
        lampShader.setInt("numSpotLights", (int)lampLights.size());
        for(int i=0; i<lampLights.size(); ++i){
            lampLights[i].ApplyToShader(lampShader, "spotLights[" + std::to_string(i) + "]");
        }
        lampShader.setMat4("view",       view);
        lampShader.setMat4("projection", proj);
        lampShader.setVec3("lightPos",   lightPos);
        lampShader.setVec3("viewPos",    camera.Position);
        lampShader.setVec3("lightColor", lightColor);
        lampShader.setFloat("fogStart", fogStart);
        lampShader.setFloat("fogEnd",   fogEnd);
        lampShader.setVec3("fogColor",  fogColor);
        lampShader.setInt("shadowMap", 5);
        lampShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        for (const glm::vec2& pos : lampPositions) {
                float lx = pos.x;
                float lz = pos.y;
                float ly = lodTerrain.getHeightAt(lx, lz);

                // translate so the lamp sits on the terrain
                glm::mat4 lampM = glm::translate(glm::mat4(1.0f),
                                                    glm::vec3(lx, ly, lz));
                // if your lamp model’s origin is at its base, you’re done.
                // otherwise you might need to pull it up by half its height:
                // lampM = glm::translate(lampM, glm::vec3(0, lampBaseOffset, 0));

                lampM = glm::scale(lampM, glm::vec3(1.5f));  // optional scale
                lampShader.setMat4("model", lampM);
                lamp.Draw(lampShader);
        }
        sphereShader.use();
        sphereShader.setMat4("view",       view);
        sphereShader.setMat4("projection", proj);
        sphereShader.setVec3("color", glm::vec3(1.0f, 0.85f, 0.6f));
        std::vector<glm::vec3> bulbWorldPositions;
        bulbWorldPositions.reserve(lampPositions.size() * bulbLocals.size());

        for (auto const& pos : lampPositions) {
            float lx = pos.x, lz = pos.y;
            float ly = lodTerrain.getHeightAt(lx, lz);

            // same matrix you use to draw the lamp model itself
            glm::mat4 lampM = glm::translate(glm::mat4(1.0f),
                                            glm::vec3(lx, ly, lz));
            lampM = glm::scale(lampM, glm::vec3(1.5f));  // your chosen scale

        // transform each local bulb into world
            for (auto const& local : bulbLocals) {
                bulbWorldPositions.push_back(glm::vec3(lampM * local));
            }
        }

        for (auto const & bulbPos : bulbWorldPositions) {
            glm::mat4 M = glm::translate(glm::mat4(1.0f), bulbPos)
                        * glm::scale    (glm::mat4(1.0f), glm::vec3(0.1f));  // tweak sphere size
            sphereShader.setMat4("model", M);
            lightSphere.draw();
        }

        litShader.use();
        litShader.setMat4("view", camera.GetViewMatrix());
        litShader.setMat4("projection", proj);
        litShader.setVec3("lightDir", glm::normalize(lightPos));
        litShader.setVec3("lightPos", lightPos);
        litShader.setVec3("viewPos", camera.Position);
        litShader.setVec3("lightColor", lightColor);
        
        lightViz.Draw(proj, view, lightPos, sphereScale);


        // 1c) skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(camera.GetViewMatrix())));
        skyboxShader.setMat4("projection", proj);
        skybox.render();
        glDepthFunc(GL_LESS);

        // restore camera
        camera.Position.y += d;
        camera.Pitch = -camera.Pitch;
        water.UnbindFrameBuffer(SCR_WIDTH, SCR_HEIGHT);
        glDisable(GL_CLIP_DISTANCE0);

        //
        // 2) REFRACTION
        //
        glm::vec4 clipPlaneF = glm::vec4(0, -1, 0, WATER_HEIGHT + 0.8);
        water.BindRefractionFrameBuffer();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // draw only what's under water:
        glEnable(GL_CLIP_DISTANCE0);
        // — bind terrain shader —
        terrainShader.use();


        terrainShader.setInt("numSpotLights", (int)lampLights.size());
        for(int i=0; i<lampLights.size(); ++i){
            lampLights[i].ApplyToShader(terrainShader, "spotLights[" + std::to_string(i) + "]");
        }
        // textures
        terrainShader.setInt("albedoMap",   0);
        terrainShader.setInt("normalMap",   1);

        // lighting
        terrainShader.setVec3("lightDir", glm::normalize(-lightPos));
        terrainShader.setVec3("lightColor", lightColor);
        terrainShader.setFloat("ambientStrength", ambientStrength);
        terrainShader.setVec3("viewPos",    camera.Position);
        terrainShader.setFloat("dayFactor", dayFactor);
        terrainShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        terrainShader.setBool("shadowsEnabled", lightPos.y > 0.0f);

        // fog
        terrainShader.setFloat("fogStart", fogStart);
        terrainShader.setFloat("fogEnd",   fogEnd);
        terrainShader.setVec3 ("fogColor", fogColor);

        // water tint
        terrainShader.setFloat("waterHeight", WATER_HEIGHT);
        terrainShader.setFloat("maxDepth",     25.0f);
        terrainShader.setVec3 ("shallowColor", glm::vec3(0.0f,0.25f,0.4f));
        terrainShader.setVec3 ("deepColor",    glm::vec3(0.0f,0.05f,0.2f));

        // transforms + clip
        terrainShader.setMat4("model",      model);
        terrainShader.setMat4("view",       view);
        terrainShader.setMat4("projection", proj);
        terrainShader.setVec4("clipPlane",  clipPlaneF);

        // tiling scale
        terrainShader.setFloat("worldScale", worldSize);

        // draw
        lodTerrain.Draw(camera.Position);


        glDisable(GL_CLIP_DISTANCE0);
        water.UnbindFrameBuffer(SCR_WIDTH, SCR_HEIGHT);

        //
        // 3) MAIN ONSCREEN PASS
        //
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 3a) terrain (no clipping)

        // — bind terrain shader —
        terrainShader.use();
        terrainShader.setInt("numSpotLights", (int)lampLights.size());
        for(int i=0; i<lampLights.size(); ++i){
            lampLights[i].ApplyToShader(terrainShader, "spotLights[" + std::to_string(i) + "]");
        }

        // textures
        terrainShader.setInt("albedoMap",   0);
        terrainShader.setInt("normalMap",   1);
        terrainShader.setInt("shadowMap", 5);
        terrainShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        // lighting
        terrainShader.setVec3("lightDir", glm::normalize(lightPos));
        terrainShader.setVec3("lightColor", lightColor);
        terrainShader.setFloat("ambientStrength", ambientStrength);
        terrainShader.setVec3("viewPos",    camera.Position);
        terrainShader.setFloat("dayFactor", dayFactor);
        terrainShader.setBool("shadowsEnabled", lightPos.y > 0.0f);

        // fog
        terrainShader.setFloat("fogStart", fogStart);
        terrainShader.setFloat("fogEnd",   fogEnd);
        terrainShader.setVec3 ("fogColor", fogColor);

        // water tint
        terrainShader.setFloat("waterHeight", WATER_HEIGHT);
        terrainShader.setFloat("maxDepth",     25.0f);
        terrainShader.setVec3 ("shallowColor", glm::vec3(0.0f,0.25f,0.4f));
        terrainShader.setVec3 ("deepColor",    glm::vec3(0.0f,0.05f,0.2f));

        // transforms + clip
        terrainShader.setMat4("model",      model);
        terrainShader.setMat4("view",       view);
        terrainShader.setMat4("projection", proj);
        terrainShader.setVec4("clipPlane",  clipPlaneR);

        // tiling scale
        terrainShader.setFloat("worldScale", worldSize);

        // draw
        lodTerrain.Draw(camera.Position);


        lampShader.use();
        lampShader.use();
        lampShader.setInt("numSpotLights", (int)lampLights.size());
        for(int i=0; i<lampLights.size(); ++i){
            lampLights[i].ApplyToShader(lampShader, "spotLights[" + std::to_string(i) + "]");
        }
        lampShader.setMat4("view",       view);
        lampShader.setMat4("projection", proj);
        lampShader.setVec3("lightPos",   lightPos);
        lampShader.setVec3("viewPos",    camera.Position);
        lampShader.setVec3("lightColor", lightColor);
        lampShader.setFloat("fogStart", fogStart);
        lampShader.setFloat("fogEnd",   fogEnd);
        lampShader.setVec3("fogColor",  fogColor);
        lampShader.setInt("shadowMap", 5);
        lampShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        for (const glm::vec2& pos : lampPositions) {
                float lx = pos.x;
                float lz = pos.y;
                float ly = lodTerrain.getHeightAt(lx, lz);

                // translate so the lamp sits on the terrain
                glm::mat4 lampM = glm::translate(glm::mat4(1.0f),
                                                    glm::vec3(lx, ly, lz));
                // if your lamp model’s origin is at its base, you’re done.
                // otherwise you might need to pull it up by half its height:
                // lampM = glm::translate(lampM, glm::vec3(0, lampBaseOffset, 0));

                lampM = glm::scale(lampM, glm::vec3(1.5f));  // optional scale
                lampShader.setMat4("model", lampM);
                lamp.Draw(lampShader);
        }
        sphereShader.use();
        sphereShader.setMat4("view",       view);
        sphereShader.setMat4("projection", proj);
        sphereShader.setVec3("color", glm::vec3(1.0f, 0.85f, 0.6f));
        bulbWorldPositions.reserve(lampPositions.size() * bulbLocals.size());

        for (auto const& pos : lampPositions) {
            float lx = pos.x, lz = pos.y;
            float ly = lodTerrain.getHeightAt(lx, lz);

            // same matrix you use to draw the lamp model itself
            glm::mat4 lampM = glm::translate(glm::mat4(1.0f),
                                            glm::vec3(lx, ly, lz));
            lampM = glm::scale(lampM, glm::vec3(1.5f));  // your chosen scale

        // transform each local bulb into world
            for (auto const& local : bulbLocals) {
                bulbWorldPositions.push_back(glm::vec3(lampM * local));
            }
        }
        for (auto const & bulbPos : bulbWorldPositions) {
            glm::mat4 M = glm::translate(glm::mat4(1.0f), bulbPos)
                        * glm::scale    (glm::mat4(1.0f), glm::vec3(0.1f));  // tweak sphere size
            sphereShader.setMat4("model", M);
            lightSphere.draw();
        }
        treeShader.use();
        treeShader.setMat4("view",       view);
        treeShader.setMat4("projection", proj);
        treeShader.setVec3("lightPos",   lightPos);
        treeShader.setVec3("viewPos",    camera.Position);
        treeShader.setVec3("lightColor", lightColor);
        treeShader.setFloat("fogStart", fogStart);
        treeShader.setFloat("fogEnd",   fogEnd);
        treeShader.setVec3("fogColor",  fogColor);
        treeShader.setInt("shadowMap", 5);
        treeShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        for (const glm::vec2& pos : treePositions) {
            float wx = pos.x;
            float wz = pos.y;
            float wy = lodTerrain.getHeightAt(wx, wz);

            glm::mat4 treeModel = glm::mat4(1.0f);
            // Dịch origin của cây đến y = wy – treeBaseOffset
            treeModel = glm::translate(treeModel,
                                       glm::vec3(wx, wy - treeBaseOffset, wz));
            // Scale cây (nếu muốn)
            treeModel = glm::scale(treeModel, glm::vec3(1.5f));

            treeShader.setMat4("model", treeModel);
            tree.Draw(treeShader);
        }


        litShader.use(); // Use same shader as main pass
        litShader.setMat4("view", camera.GetViewMatrix());
        litShader.setMat4("projection", proj);
        litShader.setVec3("lightDir", glm::normalize(-lightPos));
        litShader.setVec3("lightPos", lightPos);
        litShader.setVec3("viewPos", camera.Position);
        litShader.setVec3("lightColor", lightColor);
        lightViz.Draw(proj, view, lightPos, sphereScale);


        // 3c) skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
        skyboxShader.setMat4("projection", proj);
        skybox.render();
        glDepthFunc(GL_LESS);

        // 3d) water (blended on top)

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        water.Draw(
            glm::translate(glm::mat4(1.0f), glm::vec3(0, WATER_HEIGHT, 0)),
            view,
            proj,
            camera.Position,
            lightPos,
            lightColor,
            dudvMove,
            skybox.getTextureID());

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // — ImGui overlay —
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Control Panel");

        // 1) Sun / Day-Night controls
        ImGui::Text("Sun Cycle:");
        ImGui::Text("  Light Position: %.1f, %.1f, %.1f",
                    lightPos.x, lightPos.y, lightPos.z);
        ImGui::SliderFloat("Day/Night Cycle (sec)",
                        &dayNightCycleSeconds,
                        5.0f, 300.0f,
                        "%.1f s");

        ImGui::Separator();
        ImGui::Text("View Presets:");
        if (ImGui::Button("Default View"))
        {
            camera.Position = glm::vec3(10.0f, 165.0f, 35.0f);
            camera.Yaw = -90.0f;
            camera.Pitch = -40.0f;
            camera.SetViewPreset(glm::vec3(10.0f, 165.0f, 35.0f), -90.0f, -60.0f);
        }
        if (ImGui::Button("Top View"))
        {
            camera.Position = glm::vec3(0.0f, 800.0f, 0.1f);
            camera.Yaw = -90.0f;
            camera.Pitch = -89.9f;
            camera.SetViewPreset(glm::vec3(0.0f, 800.0f, 0.1f), -90.0f, -89.9f);
        }
        if (ImGui::Button("Side View"))
        {
            camera.Position = glm::vec3(0.0f, 5.0f, 700.0f);
            camera.Yaw = -90.0f;
            camera.Pitch = 0.0f;
            camera.SetViewPreset(glm::vec3(0.0f, 5.0f, 700.0f), -90.0f, 0.0f);
        }
        if (ImGui::Button("Front View"))
        {
            camera.Position = glm::vec3(700.0f, 5.0f, 0.0f);
            camera.Yaw = 180.0f;
            camera.Pitch = 0.0f;
            camera.SetViewPreset(glm::vec3(700.0f, 5.0f, 0.0f), 180.0f, 0.0f);
        }
        
        

        // 3) Fog
        ImGui::Separator();
        ImGui::Text("Fog:");
        ImGui::SliderFloat("  Start", &fogStart, 0.0f, 1000.0f);
        ImGui::SliderFloat("  End",   &fogEnd,   fogStart, 2000.0f);
        ImGui::ColorEdit3("  Color", glm::value_ptr(fogColor));

        // 4) Camera info
        ImGui::Separator();
        ImGui::Text("Camera:");
        ImGui::Text("  Position: %.1f, %.1f, %.1f",
                    camera.Position.x,
                    camera.Position.y,
                    camera.Position.z);
        ImGui::Text("  Pitch: %.1f°, Yaw: %.1f°",
                    camera.Pitch, camera.Yaw);
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // 5) Debug FBOs, shadow
        ImGui::Separator();           
        ImGui::Checkbox("Show Map Debug", &showDebug); 
        ImGui::End();
        if(showDebug){
            if(ImGui::Begin("Debug section", &showDebug)){
                ImGui::Separator();
                ImGui::Text("Shadow Map Debug");
                ImGui::Image((ImTextureID)(uintptr_t)depthMap, ImVec2(256,256),
                            ImVec2(0,1), ImVec2(1,0));
                
                ImGui::Separator();
                ImGui::Text("Debug FBO Textures:");
                ImVec2 thumbsz(160,120);

                ImGui::Text("Reflection:");
                ImGui::Image((ImTextureID)(uintptr_t)water.getReflectionTexture(),
                            thumbsz, ImVec2(0,1), ImVec2(1,0));

                ImGui::Text("Refraction:");
                ImGui::Image((ImTextureID)(uintptr_t)water.getRefractionTexture(),
                            thumbsz, ImVec2(0,1), ImVec2(1,0));
                
            }
            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // — Cleanup —
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

// — process input and callbacks —
void processInput(GLFWwindow *w)
{
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(w, true);
    float sp = (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 25.0f : 5.0f);
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime * sp);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime * sp);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime * sp);
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime * sp);
}
void framebuffer_size_callback(GLFWwindow *, int w, int h)
{
    glViewport(0, 0, w, h);
}
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (!camera.LeftMousePressed)
        return;
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float dx = xpos - lastX, dy = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(dx, dy);
}
void scroll_callback(GLFWwindow *, double, double y)
{
    camera.ProcessMouseScroll((float)y);
}
void mouse_button_callback(GLFWwindow *window, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            camera.LeftMousePressed = true;
            firstMouse = true;
        }
        else if (action == GLFW_RELEASE)
        {
            camera.LeftMousePressed = false;
        }
    }
}
