// vk_buffer.cpp
#include "common/engine_pch.h"
#include "vk_buffer.h"
#include "vk_context.h"

namespace ix 
{

    VulkanBuffer::VulkanBuffer(VulkanContext& context,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags allocFlags)
        : m_context(context), m_instanceSize(instanceSize)
    {
        m_allocator = m_context.getAllocator();
        m_bufferSize = instanceSize * instanceCount;

        VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = m_bufferSize;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vmaAllocInfo{};
        vmaAllocInfo.usage = memoryUsage;
        vmaAllocInfo.flags = allocFlags;

        if (vmaCreateBuffer(m_allocator, &bufferInfo, &vmaAllocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("VulkanBuffer: Failed to create buffer!");
        }
    }

    VulkanBuffer::~VulkanBuffer() 
    {
        if (m_buffer != VK_NULL_HANDLE) {

            if (m_mappedPtr != nullptr) {
                unmap();
            }
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);

            m_buffer = VK_NULL_HANDLE;
            m_allocation = VK_NULL_HANDLE;
        }
    }

    VkResult VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) 
    {
        return vmaMapMemory(m_allocator, m_allocation, &m_mappedPtr);
    }

    void VulkanBuffer::unmap() 
    {
        if (m_mappedPtr) {
            vmaUnmapMemory(m_allocator, m_allocation);
            m_mappedPtr = nullptr;
        }
    }

    void VulkanBuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset) 
    {
        if (size == VK_WHOLE_SIZE) {
            memcpy(m_mappedPtr, data, m_bufferSize);
        }
        else {
            char* memPtr = (char*)m_mappedPtr;
            memPtr += offset;
            memcpy(memPtr, data, size);
        }
    }
    void VulkanBuffer::uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset) 
    {
        if (size == VK_WHOLE_SIZE) size = m_bufferSize;

        // Check if this buffer is actually CPU visible
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(m_allocator, m_allocation, &allocInfo);

        VkMemoryPropertyFlags memFlags;
        vmaGetMemoryTypeProperties(m_allocator, allocInfo.memoryType, &memFlags);

        if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // Direct path: Map and write
            map(size, offset);
            writeToBuffer((void*)data, size, offset);
            unmap();
        }
        else {
            // Staging path: Create a temporary host-visible buffer
            VulkanBuffer stagingBuffer(m_context, size, 1,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

            stagingBuffer.map();
            stagingBuffer.writeToBuffer((void*)data, size);
            stagingBuffer.unmap();

            // Use a one-time command buffer to copy from staging to this (GPU_ONLY) buffer
            copyBuffer(m_context, stagingBuffer.getBuffer(), m_buffer, size, offset);
        }
    }

    void VulkanBuffer::copyBuffer(VulkanContext& context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset) 
    {
        context.immediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = dstOffset;
            copyRegion.size = size;

            vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
            });
    }

    VkBuffer VulkanBuffer::getBuffer() const 
    {
        return m_buffer;
    }

    VkDescriptorBufferInfo VulkanBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) 
    {
        return VkDescriptorBufferInfo{ m_buffer, offset, size };
    }
}
