#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textureArray[];

layout(push_constant) uniform Push 
{
    mat4 modelMatrix;
    int textureIndex;
} push;

void main()
{
    vec4 texColor = texture(textureArray[nonuniformEXT(push.textureIndex)], fragUV);
    outColor = texColor;
}