#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "platform/rendering/vk/vk_buffer.h"

namespace ix
{

    struct Vertex 
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    struct VulkanMesh 
    {
        VulkanMesh() = default;
        ~VulkanMesh() = default;

        std::unique_ptr<VulkanBuffer> vertexBuffer;
        std::unique_ptr<VulkanBuffer> indexBuffer;
        uint32_t vertexCount{};
        uint32_t indexCount{};
    };
}
