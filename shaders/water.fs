#version 330 core
in  vec4 clipSpace;
in  vec2 uv;
in  vec3 worldPos;
in  vec3 worldN;

uniform sampler2D texReflect;
uniform sampler2D texRefract;
uniform sampler2D texDudv;
uniform sampler2D texNormal;
uniform sampler2D texDepthRefract;
uniform samplerCube texSkybox;

uniform vec3  eyePoint;
uniform vec3  lightPos;
uniform vec3  lightColor;
uniform float dudvMove;

out vec4 fragColor;

const float alpha            = 1;
const float shineDamper      = 300.0;
const float F0               = 0.04;

// ThinMatrix distortion params
const float distortionStrength = 0.12;
const float distortionScale    = 0.25;
const float distortionBias     = 0.25;

// Fresnel Schlick
float fresnelSchlick(float cosTheta) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Linearize depth helper
float linearizeDepth(float d) {
    float near = 0.1, far = 200.0;
    float z = d * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    // 1) Calculate basic distortion UVs
    vec2 baseDistort = texture(texDudv, vec2(uv.x + dudvMove, uv.y)).rg * 0.1;
    vec2 distortedUV = uv + baseDistort;

    // 2) ThinMatrix two‐wave distortion
    vec2 d1 = (texture(texDudv, distortedUV).rg * 2.0 - 1.0) * distortionStrength;
    vec2 d2 = (texture(texDudv, vec2(-distortedUV.x, distortedUV.y - dudvMove)).rg * 2.0 - 1.0) * distortionStrength;
    vec2 finalDistort = d1 + d2;

    // 3) Project to screen‐space NDC
    vec2 ndc    = clipSpace.xy / clipSpace.w * 0.5 + 0.5;
    vec2 uvRefl = vec2(ndc.x, -ndc.y) + finalDistort;
    vec2 uvRefr = ndc + finalDistort;

    // 4) Scale & bias UVs
    uvRefl = clamp(uvRefl * distortionScale + distortionBias, 0.001, 0.999);
    uvRefr = clamp(uvRefr * distortionScale + distortionBias, 0.001, 0.999);

    // 5) Sample reflection & refraction FBOs
    vec4 colRefl = texture(texReflect, uvRefl);
    vec4 colRefr = texture(texRefract, uvRefr);

    // 6) Depth‐based tint
    float dFactor = linearizeDepth(texture(texDepthRefract, ndc).r);
    vec4 deep = vec4(0.003, 0.109, 0.172, 0);
    vec4 sub  = vec4(0.054, 0.345, 0.392, 0);
    vec4 waterColor = mix(sub, deep, dFactor);
    vec4 refracted = mix(colRefr, waterColor, 0.5);

    // 7) Your original Fresnel mix
    vec3 Vdir = normalize(eyePoint - worldPos);
    float Fmix = fresnelSchlick(max(dot(Vdir, vec3(0,1,0)), 0.0));
    vec4 base  = mix(refracted, colRefl, Fmix);

    // 8) Specular highlights using distorted normals
    vec3 N    = normalize(texture(texNormal, uv + finalDistort).rgb * 2.0 - 1.0);
    vec3 L    = normalize(lightPos - worldPos);
    vec3 H    = normalize(L + Vdir);
    float spec= pow(max(dot(N,H),0.0), shineDamper);
    vec4 specCol = vec4(lightColor * spec * 0.5, 0.0);

    // 9) Final compose
    fragColor = base + specCol;
    fragColor.a = alpha;
}
