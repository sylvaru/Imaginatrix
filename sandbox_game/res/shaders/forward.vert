// forward_pass.vert
#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint outTextureIndex; 


layout(push_constant) uniform DrawConstants {
    uint baseInstanceOffset; 
} drawConstants;

layout(set = 0, binding = 0) uniform GlobalUbo 
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
    float time;
    float deltaTime;
    float skyboxIntensity;
    float _padding;
} ubo;

struct InstanceData 
{
    mat4 modelMatrix;
    uint textureIndex;
    float boundingRadius;
    uint batchID;
    uint _padding;
};

layout(std430, set = 2, binding = 2) readonly buffer InstanceBuffer 
{
    InstanceData instances[];
} instanceData;

void main() 
{
    InstanceData inst = instanceData.instances[gl_InstanceIndex];
    mat4 model = inst.modelMatrix;
    outTextureIndex = inst.textureIndex;

    gl_Position = ubo.projection * ubo.view * model * vec4(inPosition, 1.0);
    fragUV = inTexCoord;
}