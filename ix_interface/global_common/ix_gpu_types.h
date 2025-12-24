// ix_gpu_types.h
#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace ix 
{
    class VulkanPipelineManager;
    class AssetManager;

    struct GlobalUbo 
    {
        alignas(16) glm::mat4 projection{ 1.f };
        alignas(16) glm::mat4 view{ 1.f };
        alignas(16) glm::vec4 cameraPos{ 0.f };
        alignas(16) float time{ 0.0f };
        alignas(16) float deltaTime{ 0.0f };
    };

    struct FrameContext 
    {
        float deltaTime;
        float totalTime;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec3 cameraPosition;
        uint32_t frameIndex;
        VkCommandBuffer commandBuffer;
        VkDescriptorSet globalDescriptorSet;
        class AssetManager* assetManager;
        class VulkanPipelineManager* pipelineManager;
   
    };
}