// ix_gpu_types.h
#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace ix 
{
    class VulkanPipelineManager;
    class AssetManager;
    class VulkanDescriptorManager;
    class VulkanContext;

    struct GlobalUbo
    {
        alignas(16) glm::mat4 projection{ 1.f };
        alignas(16) glm::mat4 view{ 1.f };
        alignas(16) glm::vec4 cameraPos{ 0.f };

        float time{ 0.0f };
        float deltaTime{ 0.0f };
        float skyboxIntensity{ 1.0f };
        float padding;
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
        VkDescriptorSet bindlessDescriptorSet;
        VkDescriptorSetLayout computeStorageLayout;

        VulkanContext* context;
        AssetManager* assetManager;
        VulkanPipelineManager* pipelineManager;
        VulkanDescriptorManager* descriptorManager;
    };

    struct RenderGraphCompileConfig
    {
        VulkanContext* context;
        VulkanPipelineManager* pipelineManager;
    };

}