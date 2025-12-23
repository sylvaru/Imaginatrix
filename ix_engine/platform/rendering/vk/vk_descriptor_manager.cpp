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

    void DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count, VkDescriptorType type) 
    {
        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstBinding = binding;
        write.descriptorCount = count;
        write.descriptorType = type;
        write.pBufferInfo = bufferInfo;
        m_writes.push_back(write);
    }

    void DescriptorWriter::updateSet(VulkanContext& context, VkDescriptorSet set) 
    {
        for (auto& w : m_writes) w.dstSet = set;
        vkUpdateDescriptorSets(context.device(), (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);
    }
}