#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;        // single clip‐plane for reflection

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

out float gl_ClipDistance[1];

void main() {
    // world position + normal (as in lit.vs)
    vec4 worldPos = model * vec4(aPos,1.0);
    vs_out.FragPos   = worldPos.xyz;
    vs_out.Normal    = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;

    // clip everything *below* the plane
    gl_ClipDistance[0] = dot(worldPos, clipPlane);

    gl_Position = projection * view * worldPos;
}
