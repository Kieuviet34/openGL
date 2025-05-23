#version 330 core
out vec4 FragColor;
in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs;

uniform sampler2D diffuseMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;    // e.g. white

void main() {
    // sample texture (with alpha test for leaves)
    vec4 tex = texture(diffuseMap, fs.TexCoords);
    if (tex.a < 0.1) discard;

    vec3 color = tex.rgb;
    vec3 N = normalize(fs.Normal);
    vec3 L = normalize(lightPos - fs.FragPos);
    float diffFront  = max(dot(N, L), 0.0);
    float diffBack   = max(dot(-N, L), 0.0);
    float diff       = max(diffFront, diffBack);
    vec3 diffuse     = diff * tex.rgb * lightColor;

    vec3 V = normalize(viewPos - fs.FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N,H),0.0), 32.0);
    vec3 specular = spec * lightColor * 0.3;

    vec3 ambient = 0.1 * color;
    FragColor = vec4(ambient + diffuse + specular, tex.a);
}
