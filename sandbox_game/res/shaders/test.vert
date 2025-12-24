#version 450


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec2 fragUV;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
    float time;
    float deltaTime;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureIndex;
} push;

void main() {
    // Standard MVP transformation
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(inPosition, 1.0);
    
    // Passing color through (or you can use normals for basic lighting later)
    fragUV = inTexCoord;
}