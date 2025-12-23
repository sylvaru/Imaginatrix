// vk_resource_types.cpp
#include "common/engine_pch.h"
#include "vk_resource_types.h"


namespace ix
{
    std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
        return {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv) },
            { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) }
        };
    }

    std::vector<VkVertexInputBindingDescription> Vertex::getBindingDescriptions() {
        return { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
    }
}