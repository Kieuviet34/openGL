#version 330 core
layout(location = 0) in vec3 vtxCoord;
layout(location = 1) in vec2 vtxUv;
layout(location = 2) in vec3 vtxN;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

out vec4 clipSpace;
out vec2 uv;
out vec3 worldPos;
out vec3 worldN;

void main() {
    vec4 world = M * vec4(vtxCoord, 1.0);
    worldPos = world.xyz;
    worldN   = normalize((inverse(transpose(M)) * vec4(vtxN,0.0)).xyz);

    clipSpace = P * V * world;
    gl_Position = clipSpace;

    uv = vtxUv;
}
