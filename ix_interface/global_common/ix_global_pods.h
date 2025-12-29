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

    struct GPUIndirectCommand
    {
        uint32_t indexCount;      // 4 bytes  - Number of indices to draw
        uint32_t instanceCount;   // 4 bytes  - Number of instances to draw (updated by Compute)
        uint32_t firstIndex;      // 4 bytes  - First index in the index buffer
        int32_t  vertexOffset;    // 4 bytes  - Value added to each index before addressing vertex buffer
        uint32_t firstInstance;   // 4 bytes  - Instance index for the first instance drawn (0 in our setup)
        uint32_t _padding[11];    // 44 bytes - Pad to 64 bytes total for alignment and batch-indexing

        // Total: 64 bytes
        // This padding ensures that each command in a buffer is 64-byte aligned, 
        // matching the stride used in vkCmdDrawIndexedIndirect and GLSL culledData.
    };

    struct alignas(16) GPUInstanceData 
    {
        glm::mat4 modelMatrix;    // 64 bytes
        uint32_t  textureIndex;   // 4 bytes
        float     boundingRadius; // 4 bytes
        uint32_t  batchID;        // 4 bytes
        uint32_t  _padding;       // 4 bytes (Makes struct 80 bytes total)
    };

    struct CullingPushConstants
    {
        glm::mat4 viewProj;          // 64 bytes - Camera View-Projection matrix
        uint32_t  maxInstances;      // 4 bytes  - Total instances to process
        uint32_t  debugCulling;      // 4 bytes  - Toggle for culling visualization
        uint32_t  batchOffsets[16];  // 64 bytes - Start index for each batch in output
        // Total: 136 bytes 
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
        VkDescriptorSetLayout computeCullingLayout;
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
        VkDescriptorSet cullingDescriptorSet;

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

    struct FrameDescriptorSetGroup 
    {
        VkDescriptorSet globalSet;      // Set 0: Global UBO
        VkDescriptorSet instanceSet;    // Set 2: Culled Instance Data (For Forward Pass)
        VkDescriptorSet cullingSet;     // Set 2: Input + Output Data (For Compute Pass)
    };


}