#include "lib/glad.h"
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ultis/shaderReader.h"
#include "ultis/camera.h"

const int WIDTH = 1080;
const int HEIGHT = 1920;

void processInput(GLFWwindow * window);
void framebuffer_size_callback(GLFWwindow * window, int width, int height);
void mouse_callback(GLFWwindow * window, double xpos, double ypos);
void scroll_callback(GLFWwindow * window, double xoffset, double yoffset);
    
int main(){
    
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Map generated tool", NULL, NULL);
    if(window == NULL){
        glfwTerminate();
        return -1;
    }

    
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        return -1;
    }


    while(!glfwWindowShouldClose(window)){



    }
    return 0;
}