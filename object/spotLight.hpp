// SpotLight.hpp
#pragma once
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"

class SpotLight {
public:
    glm::vec3 Position;
    glm::vec3 Direction;
    glm::vec3 Color;

    float CutOff;       // cos(innerAngle)
    float OuterCutOff;  // cos(outerAngle)
    float Constant, Linear, Quadratic;

    bool Enabled = false;

    SpotLight(
        glm::vec3 color = glm::vec3(1.0f),
        float innerAngle = 12.5f, // degrees
        float outerAngle = 17.5f  // degrees
    ) {
        Color = color;
        CutOff = glm::cos(glm::radians(innerAngle));
        OuterCutOff = glm::cos(glm::radians(outerAngle));
        Constant = 1.0f;
        Linear   = 0.09f;
        Quadratic= 0.032f;
    }

    // Gửi uniform vào shader
    void setUniforms(const Shader& shader, const std::string& name) const {
        shader.setBool (name + ".enabled",    Enabled);
        shader.setVec3 (name + ".position",   Position);
        shader.setVec3 (name + ".direction",  Direction);
        shader.setVec3 (name + ".color",      Color);
        shader.setFloat(name + ".cutOff",     CutOff);
        shader.setFloat(name + ".outerCutOff",OuterCutOff);
        shader.setFloat(name + ".constant",   Constant);
        shader.setFloat(name + ".linear",     Linear);
        shader.setFloat(name + ".quadratic",  Quadratic);
    }
};
