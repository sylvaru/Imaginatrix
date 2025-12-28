// vk_descriptor_manager.cpp
#include "common/engine_pch.h"
#include "vk_descriptor_manager.h"
#include "vk_context.h"

namespace ix 
{

    VulkanDescriptorManager::VulkanDescriptorManager(VulkanContext& context) : m_context(context) {}

    VulkanDescriptorManager::~VulkanDescriptorManager() 
    {
        for (auto p : m_usedPools) vkDestroyDescriptorPool(m_context.device(), p, nullptr);
        for (auto p : m_freePools) vkDestroyDescriptorPool(m_context.device(), p, nullptr);
    }

    VkDescriptorPool VulkanDescriptorManager::getPool() 
    {
        if (!m_freePools.empty()) {
            VkDescriptorPool p = m_freePools.back();
            m_freePools.pop_back();
            return p;
        }
        return createPool(1000, m_poolSizes);
    }

    VkDescriptorPool VulkanDescriptorManager::createPool(int count, PoolSizes sizes) 
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (auto sz : sizes.sizes) {
            poolSizes.push_back({ sz.first, uint32_t(sz.second * count) });
        }

        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags = 0;
        poolInfo.maxSets = count;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool p;
        vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &p);
        return p;
    }

    bool VulkanDescriptorManager::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout) 
    {
        if (m_currentPool == VK_NULL_HANDLE) {
            m_currentPool = getPool();
            m_usedPools.push_back(m_currentPool);
        }

        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_currentPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkResult result = vkAllocateDescriptorSets(m_context.device(), &allocInfo, set);

        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            m_currentPool = getPool();
            m_usedPools.push_back(m_currentPool);
            allocInfo.descriptorPool = m_currentPool;
            result = vkAllocateDescriptorSets(m_context.device(), &allocInfo, set);
        }

        return result == VK_SUCCESS;
    }

    bool VulkanDescriptorManager::allocateBindless(VkDescriptorPool pool, VkDescriptorSet* set, VkDescriptorSetLayout layout, uint32_t descriptorCount)
    {
        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableAllocInfo.descriptorSetCount = 1;
        variableAllocInfo.pDescriptorCounts = &descriptorCount;
        allocInfo.pNext = &variableAllocInfo;

        return vkAllocateDescriptorSets(m_context.device(), &allocInfo, set) == VK_SUCCESS;
    }

    VkDescriptorPool VulkanDescriptorManager::createBindlessPool(uint32_t maxSets, uint32_t descriptorCount)
    {
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount }
        };

        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        // This flag allows us to update the descriptors even after they are bound to a command buffer
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool p;
        if (vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &p) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create bindless descriptor pool");
        }

        // don't add this to m_usedPools because we don't want resetPools() to touch it.
        // The AssetManager or a Global Resources class should own this lifetime.
        return p;

    }

    void VulkanDescriptorManager::resetPools() 
    {
        for (auto p : m_usedPools) {
            vkResetDescriptorPool(m_context.device(), p, 0);
            m_freePools.push_back(p);
        }
        m_usedPools.clear();
        m_currentPool = VK_NULL_HANDLE;
    }

    // DescriptorWriter
    DescriptorWriter& DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count, VkDescriptorType type)
    {
        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstBinding = binding;
        write.descriptorCount = count;
        write.descriptorType = type;
        write.pBufferInfo = bufferInfo;
        m_writes.push_back(write);

        return *this; // This allows chaining
    }

    DescriptorWriter& DescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count, VkDescriptorType type, uint32_t arrayElement)
    {
        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstBinding = binding;
        write.dstArrayElement = arrayElement;
        write.descriptorCount = count;
        write.descriptorType = type;
        write.pImageInfo = imageInfo;
        m_writes.push_back(write);

        return *this; // This allows chaining
    }

    void DescriptorWriter::updateSet(VulkanContext& context, VkDescriptorSet set)
    {
        for (auto& w : m_writes) w.dstSet = set;
        vkUpdateDescriptorSets(context.device(), (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);
    }

}