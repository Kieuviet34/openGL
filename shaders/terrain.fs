// terrain.fs
#version 330 core

in vec2  TexCoord;    // still used for normal/roughness/AO maps
in vec3  WorldPos;
in vec3  Normal;

out vec4 FragColor;

// If you still want a base rock texture for detail, keep this.
// Otherwise you can comment it out and remove the binding.
uniform sampler2D albedoMap;  
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// fog
uniform float fogStart;
uniform float fogEnd;
uniform vec3  fogColor;

// water-depth
uniform float waterHeight;    // Y coordinate of water plane
uniform float maxDepth;       // depth at which water is fully deepColor
uniform vec3  shallowColor;   // color of very shallow water
uniform vec3  deepColor;      // color of very deep water

// slope step
uniform vec3  grassColor;     // flat-ground color
uniform vec3  rockColor;      // steep-ground color
uniform float slopeThreshold; // Normal.y below this is rock

void main() {
    // SAMPLE DETAIL MAPS
    vec3 nTex = texture(normalMap, TexCoord).rgb;
    float rough = texture(roughnessMap, TexCoord).r;
    float ao    = texture(aoMap, TexCoord).r;

    // RECONSTRUCT NORMAL
    vec3 N = normalize(Normal + (nTex * 2.0 - 1.0));

    // LIGHT VECTORS
    vec3 L = normalize(lightPos - WorldPos);
    vec3 V = normalize(viewPos  - WorldPos);
    vec3 H = normalize(L + V);

    // PHONG TERMS
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * (1.0 - rough);
    float dL   = length(lightPos - WorldPos);
    float att  = 1.0 / (1.0 + 0.1 * dL + 0.01 * dL * dL);

    vec3 ambient  = 0.3 * grassColor * ao; // we’ll blend grass/rock next
    vec3 diffuse  = diff * lightColor * grassColor * att;
    vec3 specular = spec * lightColor * att;

    vec3 color = ambient + diffuse + specular;
    color = pow(color, vec3(1.0/2.2));

    // ─── SLOPE STEP ────────────────────────────────────────────────────────────
    // flat: N.y=1 → slope=0; vertical: N.y=0 → slope=1
    float slope = clamp(1.0 - N.y, 0.0, 1.0);
    float tStep = smoothstep(slopeThreshold - 0.01,
                             slopeThreshold + 0.01,
                             slope);
    // interpolate between grass and rock
    vec3 terrainColor = mix(grassColor, rockColor, tStep);
    // modulate our lighting result
    color *= terrainColor;

    // ─── WATER-DEPTH BLEND ──────────────────────────────────────────────────────
    float depth = waterHeight - WorldPos.y;            // positive when underwater
    float df    = clamp(depth / maxDepth, 0.0, 1.0);   // 0 at surface, 1 at maxDepth
    vec3  waterC = mix(shallowColor, deepColor, df);
    // only apply underwater where depth>0
    if (depth > 0.0) {
        color = mix(color, waterC, df);
    }

    // ─── FOG ───────────────────────────────────────────────────────────────────
    float viewDist = length(viewPos - WorldPos);
    float fogFactor = smoothstep(fogStart, fogEnd, viewDist);
    color = mix(color, fogColor, fogFactor);

    FragColor = vec4(color, 1.0);
}
