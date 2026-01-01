// vk_buffer.h
#pragma once
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace ix
{
    class VulkanContext;


    class VulkanBuffer 
    {
    public:
        VulkanBuffer(VulkanContext& context,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VmaMemoryUsage memoryUsage,
            VmaAllocationCreateFlags allocFlags = 0);

        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;


        VulkanBuffer(VulkanBuffer&&) = delete;
        VulkanBuffer& operator=(VulkanBuffer&&) = delete;


        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap();

        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        // Helper for one-time command submission
        static void copyBuffer(VulkanContext& context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset);

        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        VkBuffer getBuffer() const;
        VkDeviceSize getBufferSize() const { return m_bufferSize; }

    private:
        VulkanContext& m_context;
        VmaAllocator m_allocator;

        VkBuffer m_buffer = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;

        void* m_mappedPtr = nullptr;
        VkDeviceSize m_bufferSize;
        VkDeviceSize m_instanceSize;
    };
}