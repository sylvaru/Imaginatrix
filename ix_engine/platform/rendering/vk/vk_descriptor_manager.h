#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <deque>

namespace ix
{
    class VulkanContext;


    class VulkanDescriptorManager 
    {
    public:
        struct PoolSizes 
        {
            std::vector<std::pair<VkDescriptorType, float>> sizes = 
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f }
            };
        };

        VulkanDescriptorManager(VulkanContext& context);
        ~VulkanDescriptorManager();

        // High-level allocation
        bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

        // Resetting at frame start
        void resetPools();

    private:
        VkDescriptorPool getPool();
        VkDescriptorPool createPool(int count, PoolSizes sizes);

        VulkanContext& m_context;
        VkDescriptorPool m_currentPool{ VK_NULL_HANDLE };
        PoolSizes m_poolSizes;
        std::vector<VkDescriptorPool> m_usedPools;
        std::vector<VkDescriptorPool> m_freePools;
    };

    // The "Writer" handles the actual binding logic efficiently
    class DescriptorWriter 
    {
    public:
        void writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count, VkDescriptorType type);
        void writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count, VkDescriptorType type);

        void updateSet(VulkanContext& context, VkDescriptorSet set);

    private:
        std::vector<VkWriteDescriptorSet> m_writes;
    };
}