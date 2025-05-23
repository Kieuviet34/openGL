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

const float alpha = 0.2;
const float shineDamper = 300.0;
const float F0 = 0.02;

// Enhanced distortion parameters
const float distortionStrength = 0.12;
const float distortionScale = 0.25;
const float distortionBias = 0.25;

float fresnelSchlick(float cosTheta) {
    return F0 + (1.0 - F0)*pow(1.0 - cosTheta, 5.0);
}

float linearizeDepth(float d) {
    float near = 0.1, far = 200.0;
    float z = d * 2.0 - 1.0;
    float lin = (2.0*near*far)/(far+near - z*(far-near));
    return clamp(lin/far, 0.0, 1.0);
}

void main() {
    // — Enhanced distortion calculation —
    vec2 baseDistort = texture(texDudv, vec2(uv.x + dudvMove, uv.y)).rg * 0.1;
    vec2 distortedUV = uv + baseDistort;
    
    // Additional wave distortion
    vec2 d1 = (texture(texDudv, distortedUV).rg * 2.0 - 1.0) * distortionStrength;
    vec2 d2 = (texture(texDudv, vec2(-distortedUV.x, distortedUV.y - dudvMove)).rg * 2.0 - 1.0) * distortionStrength;
    vec2 finalDistort = d1 + d2;

    // — Apply distortion to UV coordinates —
    vec2 ndc = clipSpace.xy/clipSpace.w * 0.5 + 0.5;
    vec2 uvRefl = vec2(ndc.x, -ndc.y) + finalDistort;
    vec2 uvRefr = ndc.xy + finalDistort;

    // Scale and clamp coordinates
    uvRefl = clamp(uvRefl * distortionScale + distortionBias, 0.001, 0.999);
    uvRefr = clamp(uvRefr * distortionScale + distortionBias, 0.001, 0.999);

    // — fetch color from FBOs with distortion —
    vec4 colRefl = texture(texReflect, uvRefl);
    vec4 colRefr = texture(texRefract, uvRefr);

    // — depth-based color blending —
    float dFactor = linearizeDepth(texture(texDepthRefract, ndc.xy).r);
    vec4 deep = vec4(0.003, 0.109, 0.172, 0);
    vec4 sub = vec4(0.054, 0.345, 0.392, 0);
    vec4 waterColor = mix(sub, deep, dFactor);
    vec4 refracted = mix(colRefr, waterColor, 0.5);

    // — Fresnel effect —
    vec3 V = normalize(eyePoint - worldPos);
    float F = fresnelSchlick(max(dot(V, vec3(0, 1, 0)), 0.0));

    // — Final color blending —
    vec4 base = mix(refracted, colRefl, F);

    // — Specular highlights with distorted normals —
    vec3 N = normalize(texture(texNormal, uv + finalDistort).rgb * 2.0 - 1.0);
    vec3 L = normalize(lightPos - worldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), shineDamper);
    vec4 specCol = vec4(lightColor * spec * 0.5, 0.0);

    fragColor = base + specCol;
    fragColor.a = 0.9;
} 