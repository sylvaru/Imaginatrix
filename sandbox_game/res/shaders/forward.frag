// forward_pass.frag
#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint inTextureIndex;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textureArray[];

void main()
{
    vec4 texColor = texture(textureArray[(inTextureIndex)], fragUV);
    outColor = texColor;
}