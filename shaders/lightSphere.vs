#version 330 core
layout(location = 0) in vec3 aPos;
// (we don’t need the normals here, but we set them up in the mesh)
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
