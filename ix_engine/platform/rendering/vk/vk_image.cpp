// vk_image.cpp
#include "common/engine_pch.h"
#include "vk_image.h"
#include "vk_context.h"

namespace ix {
    VulkanImage::VulkanImage(VulkanContext& context, VkImage handle, VkFormat format, VkExtent2D extent)
        : m_context(context)
        , m_handle(handle)
        , m_format(format)
        , m_extent(extent)
        , m_isBorrowed(true) 
    {
        createView();
    }

    VulkanImage::~VulkanImage() 
    {
        if (m_view) vkDestroyImageView(m_context.device(), m_view, nullptr);
        if (!m_isBorrowed) 
        {
            if (m_handle) vkDestroyImage(m_context.device(), m_handle, nullptr);
            if (m_memory) vkFreeMemory(m_context.device(), m_memory, nullptr);
        }
    }

    void VulkanImage::createView() 
    {
        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = m_handle;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_view);
    }

    void VulkanImage::transition(VkCommandBuffer cmd, VkImageLayout newLayout) 
    {
        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.oldLayout = m_currentLayout;
        barrier.newLayout = newLayout;
        barrier.image = m_handle;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        // Define synchronization stages based on layout transition
        if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        }
        else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) 
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }

        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &dep);
        m_currentLayout = newLayout;
    }
}