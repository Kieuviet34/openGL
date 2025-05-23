#version 330 core

in vec2  TexCoord;
in vec3  WorldPos;
in vec3  Normal;

out vec4 FragColor;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    // Material properties
    vec3 albedo = texture(albedoMap, TexCoord).rgb;
    vec3 normal = normalize(Normal + (texture(normalMap, TexCoord).rgb * 2.0 - 1.0));
    float rough = texture(roughnessMap, TexCoord).r;
    float ao = texture(aoMap, TexCoord).r;

    // Light calculations
    vec3 lightDir = normalize(lightPos - WorldPos);
    vec3 viewDir = normalize(viewPos - WorldPos);
    vec3 halfway = normalize(lightDir + viewDir);

    // Improved lighting model
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfway), 32.0), 0.0) * (1.0 - rough);
    
    // Enhanced ambient with distance attenuation
    float distance = length(lightPos - WorldPos);
    float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
    
    vec3 ambient = vec3(0.3) * albedo * ao;
    vec3 diffuse = diff * lightColor * albedo * attenuation;
    vec3 specular = spec * lightColor * attenuation;

    // Final color with tone mapping
    vec3 result = ambient + diffuse + specular;
    result = pow(result, vec3(1.0/2.2)); // Gamma correction
    
    FragColor = vec4(result, 1.0);
}