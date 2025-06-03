#version 330 core

// =====================
// Inputs từ vertex shader:
// =====================
in vec3 WorldPos;    // vị trí fragment trong world space
in vec3 Normal;      // normal gốc
in vec2 UV;          // texture coordinates

// =====================
// Output cuối cùng:
// =====================
out vec4 FragColor;

// =====================
// Các sampler gốc:
// =====================
uniform sampler2D albedoMap;
uniform sampler2D normalMap;

// =====================
// Shadow Mapping:
// =====================
uniform sampler2D shadowMap;       // depth map từ ánh sáng (texture unit 5)
uniform mat4      lightSpaceMatrix;// ma trận lightProj * lightView

// =====================
// Lighting (Phong + ambient + day/night):
// =====================
uniform vec3 lightDir;    // hướng tới ánh sáng (đã normalize)
uniform vec3 lightColor;  // màu ánh sáng
uniform float dayFactor;  // [0..1] điều chỉnh ngày/đêm

// =====================
// Camera & Ambient:
// =====================
uniform vec3 viewPos;          // vị trí camera trong world
uniform float ambientStrength; // mức độ ambient (phần tối)

// =====================
// Fog:
// =====================
uniform float fogStart;        // khoảng cách bắt đầu fog
uniform float fogEnd;          // khoảng cách kết thúc fog
uniform vec3  fogColor;        // màu fog

// =====================
// Water Tint (nếu terrain chìm dưới nước):
// =====================
uniform float waterHeight;     // y của mặt nước
uniform float maxDepth;        // độ sâu tối đa (để pha tinted color)
uniform vec3  shallowColor;    // màu nông
uniform vec3  deepColor;       // màu sâu

// =====================
// Shadow Calculation Function
// =====================
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal)
{
    // 1) Perspective divide → NDC ánh sáng (x,y,z ∈ [-1,1])
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 2) Chuyển về [0,1] để lookup vào shadowMap
    projCoords = projCoords * 0.5 + 0.5;

    // 3) Lấy giá trị depth gần nhất từ light (shadowMap)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // 4) Depth hiện tại của fragment (so với ánh sáng)
    float currentDepth = projCoords.z;

    // 5) Tính bias để giảm shadow acne
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // 6) Nếu fragment nằm ngoài vùng chiếu của ánh sáng (projCoords.xy không ∈ [0,1]), không shadow
    if (projCoords.z > 1.0)
        return 0.0;

    // 7) Kiểm tra shadow
    float shadow = 0.0;
    if (currentDepth - bias > closestDepth)
        shadow = 1.0;

    // 8) (Tùy chọn) PCF: lấy mẫu 3×3 để mềm shadow (commented out)
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
    // 1) Sample Albedo + Normal
    // -----------------------------
    vec3 albedo    = texture(albedoMap, UV).rgb;
    vec3 nSample   = texture(normalMap, UV).rgb * 2.0 - 1.0;
    vec3 N         = normalize(mix(Normal, nSample, 0.5));

    // -----------------------------
    // 2) Phong Lighting (Lambert + Blinn‐Phong + ambient)
    // -----------------------------
    // Diffuse
    float diff = max(dot(N, -lightDir), 0.0);
    vec3 diffuse = diff * albedo * lightColor;
    diffuse *= dayFactor;  // điều chỉnh ngày/đêm

    // Specular
    vec3 V = normalize(viewPos - WorldPos);
    vec3 H = normalize(V - lightDir);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = spec * lightColor * 0.3;
    specular *= dayFactor;

    // Ambient (màu tối)
    vec3 ambient = ambientStrength * albedo;

    // Kết hợp
    vec3 lighting = ambient + diffuse + specular;

    // -----------------------------
    // 3) Water Tint (nếu có)
    // -----------------------------
    vec3 color = lighting;
    float heightDiff = waterHeight - WorldPos.y;
    if (heightDiff > 0.0)
    {
        float f = clamp(heightDiff / maxDepth, 0.0, 1.0);
        vec3 w = mix(shallowColor, deepColor, f);
        color = mix(color, w, f * 0.8);
    }

    // -----------------------------
    // 4) Shadow Mapping
    // -----------------------------
    // Tính vị trí fragment trong không gian ánh sáng
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    float shadow = ShadowCalculation(fragPosLightSpace, N);

    // Nếu shadow == 1.0 thì ta chỉ giữ ambient (loại bỏ diffuse + specular)
    color = ambient + (1.0 - shadow) * (diffuse + specular);

    // -----------------------------
    // 5) Fog
    // -----------------------------
    float dist = length(viewPos - WorldPos);
    float fogF = smoothstep(fogStart, fogEnd, dist);
    color = mix(color, fogColor, fogF);

    // -----------------------------
    // 6) Gamma correction
    // -----------------------------
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
