#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D texture_diffuse1;

void main()
{
    vec4 tex = texture(texture_diffuse1, TexCoords);
    // cut out thin, high‑contrast leaf edges exactly like grass did:
    if (tex.a < 0.1)
        discard;
    FragColor = tex;
}
