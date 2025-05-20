#version 330 core
in vec2 TexCoord;           // from vertex shader
out vec4 FragColor;

uniform sampler2D texture1; // make sure this matches the name you set in C++

void main(){
    FragColor = texture(texture1, TexCoord);
}
