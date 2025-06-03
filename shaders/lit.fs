#version 330 core

// =====================
// Inputs từ vertex shader:
// =====================
in VS_OUT {
    vec3 FragPos;    // vị trí fragment trong world space
    vec3 Normal;     // normal
    vec2 TexCoords;  // texture coordinates
} fs;

// =====================
// Output cuối cùng:
// =====================
out vec4 FragColor;

// =====================
// Các sampler gốc:
// =====================
uniform sampler2D diffuseMap;    // diffuse texture của model

// =====================
// Shadow Mapping:
// =====================
uniform sampler2D shadowMap;       // depth map từ ánh sáng (texture unit 5)
uniform mat4      lightSpaceMatrix;// ma trận lightProj * lightView

// =====================
// Lighting (Phong + ambient):
// =====================
uniform vec3 lightPos;     // vị trí ánh sáng (world space) – dùng để tính L vector
uniform vec3 viewPos;      // vị trí camera (world)
uniform vec3 lightColor;   // màu ánh sáng

// =====================
// Fog:
// =====================
uniform float fogStart;
uniform float fogEnd;
uniform vec3  fogColor;

// =====================
// Shadow Calculation Function
// =====================
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal)
{
    // 1) Perspective divide → NDC ánh sáng
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 2) Chuyển về [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // 3) Lấy depth trong shadowMap
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // 4) Depth hiện tại
    float currentDepth = projCoords.z;

    // 5) Tính bias để giảm acne
    float bias = max(0.05 * (1.0 - dot(normal, normalize(lightPos - fs.FragPos))), 0.005);

    // 6) Nếu fragment nằm ngoài vùng chiếu (projCoords.xy không ∈ [0,1]), không shadow
    if (projCoords.z > 1.0)
        return 0.0;

    // 7) Kiểm tra shadow
    float shadow = 0.0;
    if (currentDepth - bias > closestDepth)
        shadow = 1.0;

    // 8) (Tùy chọn) PCF để mềm shadow (commented)
    /*
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }
    }
    shadow /= 9.0;
    */

    return shadow;
}

void main()
{
    // -----------------------------
    // 1) Sample diffuse + discard alpha (nếu có)
    // -----------------------------
    vec4 texColor = texture(diffuseMap, fs.TexCoords);
    if (texColor.a < 0.1)
        discard; 

    vec3 color = texColor.rgb;

    // -----------------------------
    // 2) Tính ambient + diffuse + specular (Phong)
    // -----------------------------
    vec3 N = normalize(fs.Normal);
    vec3 L = normalize(lightPos - fs.FragPos);
    float diffFront = max(dot(N, L), 0.0);
    float diffBack  = max(dot(-N, L), 0.0);
    float diff      = max(diffFront, diffBack);
    vec3 diffuse    = diff * color * lightColor;

    vec3 V = normalize(viewPos - fs.FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * lightColor * 0.3;

    vec3 ambient = 0.1 * color;
    vec3 lightingColor = ambient + diffuse + specular;

    // -----------------------------
    // 3) Shadow Mapping
    // -----------------------------
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fs.FragPos, 1.0);
    float shadow = ShadowCalculation(fragPosLightSpace, N);

    // Nếu shadow == 1 → chỉ còn ambient
    vec3 result = ambient + (1.0 - shadow) * (diffuse + specular);

    // -----------------------------
    // 4) Fog
    // -----------------------------
    float dist = length(viewPos - fs.FragPos);
    float fogF = smoothstep(fogStart, fogEnd, dist);
    result = mix(result, fogColor, fogF);

    // -----------------------------
    // 5) Xuất ra (giữ alpha của texture)
    // -----------------------------
    FragColor = vec4(result, texColor.a);
}
