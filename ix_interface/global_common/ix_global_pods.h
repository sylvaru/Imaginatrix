// ix_global_pods.h
#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "common/handles.h"

namespace ix 
{
    class VulkanPipelineManager;
    class AssetManager;
    class VulkanDescriptorManager;
    class VulkanContext;
    class VulkanBuffer;

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

    struct RenderGraphCompileConfig
    {
        VulkanContext* context;
        VulkanPipelineManager* pipelineManager;
    };

    struct GPUIndirectCommand // This is what the GPU hardware reads
    {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
        uint32_t firstInstance;
    };

    struct GPUInstanceData // This is the per-object data (stored in a Storage Buffer)
    {
        glm::mat4 modelMatrix;
        uint32_t  textureIndex;
        float     boundingRadius; // For culling
        uint32_t  _padding[2];
    };

    struct RenderBatch
    {
        MeshHandle meshHandle;
        uint32_t   instanceCount;
        uint32_t   firstInstance;
    };

    struct RenderContext 
    {
        VulkanContext* context;
        AssetManager* assetManager;
        VulkanPipelineManager* pipelineManager;
        VulkanDescriptorManager* descriptorManager;
        VkDescriptorSetLayout computeStorageLayout;
    };

    // The specific "State" of the current frame
    struct FrameContext 
    {
        uint32_t frameIndex;
        VkCommandBuffer commandBuffer;

        // Volatile Descriptor Sets (Change every frame)
        VkDescriptorSet globalDescriptorSet;
        VkDescriptorSet bindlessDescriptorSet;
        VkDescriptorSet instanceDescriptorSet;

        // Batching & Culling results
        uint32_t instanceCount;
        const std::vector<RenderBatch>* renderBatches;
    };

    // Constant Data
    struct SceneView 
    {
        float deltaTime;
        float totalTime;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec3 cameraPosition;
    };

    struct RenderState 
    {
        const RenderContext& system;
        const FrameContext& frame;
        const SceneView& view;
    };

    struct DrawCommand
    {
        uint32_t pipelineID;
        uint32_t materialID;
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        uint32_t indexCount;
        glm::mat4 transform;
    };

    struct FrameData
    {
        VkSemaphore imageAvailableSemapohore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };


}