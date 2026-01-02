// skybox.vert
#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec3 outUVW;

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

vec3 pos[8] = vec3[8](
    vec3(-1, -1,  1), vec3( 1, -1,  1), vec3( 1,  1,  1), vec3(-1,  1,  1),
    vec3(-1, -1, -1), vec3( 1, -1, -1), vec3( 1,  1, -1), vec3(-1,  1, -1)
);


int indices[36] = int[](
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7,
    4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3
);

void main() {
    vec3 vertexPos = pos[indices[gl_VertexIndex]];
    
    // Flip Y
    outUVW = vec3(vertexPos.x, -vertexPos.y, vertexPos.z);
    
    mat4 viewNoTranslation = mat4(mat3(ubo.view));
    vec4 clipPos = ubo.projection * viewNoTranslation * vec4(vertexPos, 1.0);
    
    gl_Position = clipPos.xyww;
}