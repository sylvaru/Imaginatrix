// skybox.frag
#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_enhanced_layouts : enable
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;


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

// Bindless array of Cubemaps
layout(set = 1, binding = 0) uniform samplerCube skyboxTextures[];

layout(std140, push_constant) uniform Push 
{
    layout(offset = 64) int skyboxIdx; 
} push;

void main() 
{
    vec3 hdr = texture(skyboxTextures[push.skyboxIdx], normalize(inUVW)).rgb;

    // Soft highlight compression (Reinhard)
    hdr = hdr / (hdr + vec3(1.0));

    // Apply intensity after compression
    vec3 color = hdr * ubo.skyboxIntensity;
    outColor = vec4(color, 1.0);
}