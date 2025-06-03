#version 330 core

layout(location = 0) in vec3 aPos;      // local vertex position
layout(location = 1) in vec3 aNormal;   // local vertex normal

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float worldScale;    // == the total width/depth of your terrain
uniform vec4  clipPlane;     // for water‐reflection/refraction

out vec3 WorldPos;
out vec3 Normal;
out vec2 UV;

void main() {
    // 1) Compute world‐space position
    vec4 w = model * vec4(aPos,1.0);
    WorldPos = w.xyz;

    // 2) Clip plane for reflection/refraction passes
    gl_ClipDistance[0] = dot(w, clipPlane);

    // 3) Transform normal to world‐space
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // 4) Build a UV coordinate that covers [0..1] exactly once
    //    across worldScale in X and Z:
    UV = w.xz / worldScale;

    // 5) Final vertex position
    gl_Position = projection * view * w;
}
