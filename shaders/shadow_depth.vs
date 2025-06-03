#version 330 core
layout(location = 0) in vec3 aPos;   // Chỉ cần position trong mesh (không cần texcoord hay normal)

uniform mat4 lightSpaceMatrix;        // = lightProj * lightView
uniform mat4 model;                   // model matrix của object

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
