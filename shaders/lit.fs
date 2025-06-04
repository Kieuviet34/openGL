#version 330 core

// ---- inputs from vertex shader ----
in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs;

// ---- final output ----
out vec4 FragColor;

// ---- samplers ----
uniform sampler2D diffuseMap;
uniform sampler2D shadowMap;         // unit 5
uniform mat4      lightSpaceMatrix;

// ---- sun / directional light ----
uniform vec3 viewPos;
uniform vec3 lightPos;               // sun position (for specular)
uniform vec3 lightColor;             // sun color

// ---- fog ----
uniform float fogStart;
uniform float fogEnd;
uniform vec3  fogColor;

// ---- spotlight support ----
#define MAX_SPOTLIGHTS 10
struct SpotLight {
    vec3 Position;
    vec3 Direction;
    float CutOff;       // cos(innerAngle)
    float OuterCutOff;  // cos(outerAngle)
    vec3 Ambient;
    vec3 Diffuse;
    vec3 Specular;
    float Constant;
    float Linear;
    float Quadratic;
};
uniform int        numSpotLights;
uniform SpotLight  spotLights[MAX_SPOTLIGHTS];

// ---- helper: shadow calculation (PCF) ----
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N) {
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if(proj.z > 1.0) return 0.0;
    float bias = max(0.05 * (1.0 - dot(N, normalize(lightPos - fs.FragPos))), 0.005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap,0);
    for(int x=-1; x<=1; ++x){
      for(int y=-1; y<=1; ++y){
        float p = texture(shadowMap, proj.xy + vec2(x,y)*texelSize).r;
        if(proj.z - bias > p) shadow += 1.0;
      }
    }
    return shadow/9.0;
}

// ---- helper: calculate one spotlight’s contribution ----
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 fragPos, vec3 viewDir, vec3 albedo) {
    vec3 L = normalize(light.Position - fragPos);
    // diffuse
    float diff = max(dot(N, L), 0.0);
    // specular (Blinn-Phong)
    vec3 H = normalize(L + viewDir);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    // attenuation
    float dist = length(light.Position - fragPos);
    float atten = 1.0 / (light.Constant + light.Linear*dist + light.Quadratic*dist*dist);
    // spotlight intensity
    float theta = dot(L, normalize(-light.Direction));
    float epsilon = light.CutOff - light.OuterCutOff;
    float intensity = clamp((theta - light.OuterCutOff)/epsilon, 0.0, 1.0);
    // combine
    vec3 ambient  = light.Ambient  * albedo;
    vec3 diffuse  = light.Diffuse  * diff * albedo;
    vec3 specular = light.Specular * spec;
    ambient  *= atten * intensity;
    diffuse  *= atten * intensity;
    specular *= atten * intensity;
    return ambient + diffuse + specular;
}

void main() {
    // 1) sample base color
    vec4 texColor = texture(diffuseMap, fs.TexCoords);
    if(texColor.a < 0.1) discard;
    vec3 albedo = texColor.rgb;

    // 2) sun / directional lighting (Phong)
    vec3 N = normalize(fs.Normal);
    vec3 Ls = normalize(lightPos - fs.FragPos);
    float diff = max(dot(N, Ls),0.0);
    vec3 V = normalize(viewPos - fs.FragPos);
    vec3 Hs = normalize(Ls + V);
    float spec = pow(max(dot(N,Hs),0.0), 32.0);
    vec3 ambient = 0.1 * albedo;
    vec3 diffuse = diff * albedo * lightColor;
    vec3 specular = spec * lightColor * 0.3;

    // 3) shadow from sun
    vec4 posLS = lightSpaceMatrix * vec4(fs.FragPos,1.0);
    float shadow = ShadowCalculation(posLS, N);
    vec3 sunContrib = ambient + (1.0 - shadow)*(diffuse + specular);

    // 4) accumulate all spotlights
    vec3 spotContrib = vec3(0.0);
    for(int i=0; i<numSpotLights; ++i){
        spotContrib += CalcSpotLight(spotLights[i], N, fs.FragPos, V, albedo);
    }

    // 5) fog
    float d = length(viewPos - fs.FragPos);
    float f = smoothstep(fogStart, fogEnd, d);

    vec3 finalColor = mix(sunContrib + spotContrib, fogColor, f);
    FragColor = vec4(finalColor, texColor.a);
}
