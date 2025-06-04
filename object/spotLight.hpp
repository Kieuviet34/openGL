#pragma once
#include <glm/glm.hpp>
#include "../ultis/shaderReader.h"

struct SpotLight {
    // --- core spotlight parameters ---
    glm::vec3 Position   = glm::vec3(0.0f);
    glm::vec3 Direction  = glm::vec3(0.0f, -1.0f, 0.0f);

    // inner / outer cone angles in radians
    float     CutOff       = glm::cos(glm::radians(12.5f));
    float     OuterCutOff  = glm::cos(glm::radians(17.5f));

    // attenuation factors
    float     Constant     = 1.0f;
    float     Linear       = 0.09f;
    float     Quadratic    = 0.032f;

    // per‐light color channels
    glm::vec3 Ambient      = glm::vec3(0.1f);
    glm::vec3 Diffuse      = glm::vec3(1.0f);
    glm::vec3 Specular     = glm::vec3(1.0f);

    bool      Enabled      = false;

    SpotLight() = default;

    /// Construct a lamp with a single color (ambient/diffuse/specular all the same)
    SpotLight(glm::vec3 color, float innerDeg, float outerDeg)
      : CutOff(glm::cos(glm::radians(innerDeg)))
      , OuterCutOff(glm::cos(glm::radians(outerDeg)))
      , Ambient(color * 0.1f)
      , Diffuse(color)
      , Specular(color)
    {}

    /// Upload this spotlight’s uniforms into `shader` under array name e.g. "spotLights[3]"
    void ApplyToShader(const Shader &shader, const std::string &uniformBase) const {
        shader.setBool  (uniformBase + ".Enabled",     Enabled);
        shader.setVec3  (uniformBase + ".Position",    Position);
        shader.setVec3  (uniformBase + ".Direction",   Direction);
        shader.setFloat (uniformBase + ".CutOff",      CutOff);
        shader.setFloat (uniformBase + ".OuterCutOff", OuterCutOff);
        shader.setFloat (uniformBase + ".Constant",    Constant);
        shader.setFloat (uniformBase + ".Linear",      Linear);
        shader.setFloat (uniformBase + ".Quadratic",   Quadratic);
        shader.setVec3  (uniformBase + ".Ambient",     Ambient);
        shader.setVec3  (uniformBase + ".Diffuse",     Diffuse);
        shader.setVec3  (uniformBase + ".Specular",    Specular);
    }
};
