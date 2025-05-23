#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs;

uniform sampler2D diffuseMap;  // unit 0
uniform sampler2D normalMap;   // unit 1
uniform vec3      lightPos;
uniform vec3      viewPos;
uniform vec3      lightColor;

out vec4 FragColor;

void main() {
    vec4 albedo = texture(diffuseMap, fs.TexCoords);
    if (albedo.a < 0.1) discard;

    // fetch and apply normal‐map
    vec3 N = texture(normalMap, fs.TexCoords).rgb * 2.0 - 1.0;
    N = normalize(N);

    vec3 L = normalize(lightPos - fs.FragPos);
    vec3 V = normalize(viewPos   - fs.FragPos);
    vec3 H = normalize(L + V);

    // Lambert diffuse
    float diffF = max(dot(N, L),  0.0);
    float diffB = max(dot(-N, L), 0.0);
    float diff  = max(diffF, diffB);
    vec3  diffuse  = diff * albedo.rgb * lightColor;

    // Blinn–Phong specular
    float spec    = pow(max(dot(N, H), 0.0), 32.0);
    vec3  specular = spec * lightColor * 0.3;

    vec3 ambient = 0.1 * albedo.rgb;
    FragColor    = vec4(ambient + diffuse + specular, albedo.a);
}
