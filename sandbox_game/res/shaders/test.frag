#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = vec3(
        fragUV.x,                 
        fragUV.y,                 
        1.0 - fragUV.x * fragUV.y 
    );

    outColor = vec4(color, 1.0);
}