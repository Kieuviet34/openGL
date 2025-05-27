// terrain.vs
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 WorldPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float worldScale;   // NEW

void main() {
    // compute world space position
    vec4 world = model * vec4(aPos, 1.0);
    WorldPos   = world.xyz;

    // transform normal
    Normal     = mat3(transpose(inverse(model))) * aNormal;

    // use XZ world coords, scaled into [0..1]
    // if world.x runs 0 → worldScale, uv.x goes 0 → 1
    TexCoord = world.xz / worldScale;

    gl_Position = projection * view * world;
}
