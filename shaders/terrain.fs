#version 330 core

in vec3 WorldPos;
in vec3 Normal;
in vec2 UV;

out vec4 FragColor;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D shadowMap;         // unit 5
uniform mat4      lightSpaceMatrix;

// sun / directional
uniform vec3 lightDir;   // normalized from above → down
uniform vec3 lightColor;
uniform float dayFactor;

// camera & ambient
uniform vec3 viewPos;
uniform float ambientStrength;

// fog
uniform float fogStart;
uniform float fogEnd;
uniform vec3  fogColor;

// water tint
uniform float waterHeight;
uniform float maxDepth;
uniform vec3  shallowColor;
uniform vec3  deepColor;
uniform bool   shadowsEnabled;

// spotlights
#define MAX_SPOTLIGHTS 10
uniform int          numSpotLights;
struct SpotLight {
    vec3 Position;
    vec3 Direction;
    float CutOff;
    float OuterCutOff;
    vec3 Ambient;
    vec3 Diffuse;
    vec3 Specular;
    float Constant;
    float Linear;
    float Quadratic;
};
uniform SpotLight spotLights[MAX_SPOTLIGHTS];

// PCF shadow
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N) {
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj*0.5 + 0.5;
    if(proj.z>1.0 || proj.x<0.0||proj.x>1.0||proj.y<0.0||proj.y>1.0) return 0.0;
    float bias = max(0.05*(1.0 - dot(N, -lightDir)), 0.005);
    float shadow = 0.0;
    vec2 ts = 1.0/textureSize(shadowMap,0);
    for(int x=-1;x<=1;++x)
      for(int y=-1;y<=1;++y){
        float pd = texture(shadowMap, proj.xy + vec2(x,y)*ts).r;
        if(proj.z - bias > pd) shadow += 1.0;
      }
    return shadow/9.0;
}

// same spotlight helper as above
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 fragPos, vec3 viewDir, vec3 albedo){
    vec3 L = normalize(light.Position - fragPos);
    float diff = max(dot(N,L),0.0);
    vec3 H = normalize(L + viewDir);
    float spec = pow(max(dot(N,H),0.0),64.0);
    float dist = length(light.Position - fragPos);
    float atten = 1.0/(light.Constant + light.Linear*dist + light.Quadratic*dist*dist);
    float theta = dot(L, normalize(-light.Direction));
    float eps   = light.CutOff - light.OuterCutOff;
    float inten = clamp((theta - light.OuterCutOff)/eps,0.0,1.0);
    vec3 ambient  = light.Ambient  * albedo;
    vec3 diffuse  = light.Diffuse  * diff * albedo;
    vec3 specular = light.Specular * spec;
    ambient  *= atten*inten;
    diffuse  *= atten*inten;
    specular *= atten*inten;
    return ambient + diffuse + specular;
}

void main(){
    // 1) base albedo + normal
    vec3 alb = texture(albedoMap,UV).rgb;
    vec3 ns = texture(normalMap,UV).rgb*2.0 -1.0;
    vec3 N = normalize(mix(Normal, ns, 0.5));

    // 2) sun lighting (Lambert + Blinn-Phong)
    float diff = max(dot(N, -lightDir),0.0) * dayFactor;
    vec3 V = normalize(viewPos - WorldPos);
    vec3 H = normalize(V - lightDir);
    float spec = pow(max(dot(N,H),0.0),64.0)*dayFactor;
    vec3 ambient = ambientStrength * alb;
    vec3 diffuse = diff * alb * lightColor;
    vec3 specular= spec * lightColor * 0.3;
    vec3 lit = ambient + diffuse + specular;

    // 3) water tint (pre-shadow)
    float hd = waterHeight - WorldPos.y;
    if(hd>0.0){
      float f = clamp(hd/maxDepth,0.0,1.0);
      vec3 w = mix(shallowColor,deepColor,f);
      lit = mix(lit, w, f*0.8);
    }

    // 4) sun shadow
    vec4 posLS = lightSpaceMatrix * vec4(WorldPos, 1.0);
    float shadow = 0.0;
    if (shadowsEnabled) {
        shadow = ShadowCalculation(posLS, N);
    }
    // now blend only the non-ambient part with shadow
    vec3 sunContrib = ambient + (1.0 - shadow) * (lit - ambient);

    // 5) spotlights
    vec3 spotContrib = vec3(0.0);
    for(int i=0;i<numSpotLights;++i){
      spotContrib += CalcSpotLight(spotLights[i],N,WorldPos,V,alb);
    }

    // 6) fog & gamma
    float d = length(viewPos - WorldPos);
    float fogF = smoothstep(fogStart, fogEnd, d);
    vec3 finalC = mix(sunContrib + spotContrib, fogColor, fogF);
    finalC = pow(finalC, vec3(1.0/2.2));

    FragColor = vec4(finalC,1.0);
}
