// forward_pass.frag
#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint inTextureIndex;
layout(location = 2) in vec3 inViewPos;
layout(location = 3) in vec3 inViewNormal;

layout(location = 0) out vec4 outColor;


struct GPUPointLight {
    vec4 position;  // xyz = pos, w = radius
    vec4 color;     // rgb = color, w = intensity
};

struct LightGrid {
    uint offset;
    uint count;
};

// Set 0: Global
layout(set = 0, binding = 0) uniform GlobalUbo 
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
    vec2 screenResolution;
    float time;
    float deltaTime;
    float skyboxIntensity;
    float _padding;
} ubo;

layout(std430, binding = 1) readonly buffer _LightBuffer { 
    uint lightCount; 
    uint pad[3];
    GPUPointLight lights[]; 
} lightData;

layout(std430, binding = 3) readonly buffer _LightGrid { LightGrid grid[]; } lightGrid;
layout(std430, binding = 4) readonly buffer _LightIndexList { uint indexList[]; } lightIndexList;

// Set 1: Bindless Textures
layout(set = 1, binding = 0) uniform sampler2D textureArray[];

// Cluster Constants (Should match C++ ClusterConstants)
const uint gridX = 16;
const uint gridY = 9;
const uint gridZ = 24;
const float zNear = 0.1;
const float zFar = 1000.0;

void main()
{
vec4 texColor = texture(textureArray[inTextureIndex], fragUV);
    vec3 N = normalize(inViewNormal);
    vec3 accumulatedLight = vec3(0.0);

    // Cluster Math
    float viewZ = max(abs(inViewPos.z), zNear); 
    uint zSlice = uint(log(viewZ / zNear) * float(gridZ) / log(zFar / zNear));
    uint xTile = uint(gl_FragCoord.x / (ubo.screenResolution.x / float(gridX)));
    uint yTile = uint(gl_FragCoord.y / (ubo.screenResolution.y / float(gridY)));
    uint clusterIndex = clamp(xTile + (yTile * gridX) + (zSlice * (gridX * gridY)), 0, (gridX * gridY * gridZ) - 1);

    LightGrid lGrid = lightGrid.grid[clusterIndex];

    // Light Loop
    for (uint i = 0; i < lGrid.count; i++) {
        uint lightIdx = lightIndexList.indexList[lGrid.offset + i];
        GPUPointLight light = lightData.lights[lightIdx];

        vec3 lightVec = light.position.xyz - inViewPos;
        float dist = length(lightVec);
        
        if (dist < light.position.w) {
            vec3 L = normalize(lightVec);
            
            // Physically based attenuation with windowing
            float atten = 1.0 / (dist * dist + 1.0);
            float window = pow(clamp(1.0 - pow(dist / light.position.w, 4.0), 0.0, 1.0), 2.0);
            
            float nDotL = max(dot(N, L), 0.0);
            accumulatedLight += (light.color.rgb * light.color.w) * nDotL * atten * window;
        }
    }

    // Post-Processing
    vec3 ambient = texColor.rgb * 0.02;
    vec3 finalColor = ambient + (accumulatedLight * texColor.rgb);

    // Reinhard Tone Mapping & Gamma Correction
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0 / 1.9));

    outColor = vec4(finalColor, texColor.a);

    // Debug testing 

    // For this test to be visually informative render a large plane
    //float complexity = float(lGrid.count) / 16.0; // Red at 16+ lights, Green at 0
    //outColor = vec4(complexity, 1.0 - complexity, 0.0, 1.0);
}