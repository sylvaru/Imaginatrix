#include "vk_image.h"
#include "vk_context.h"
#include "vk_buffer.h"

#include <vk_mem_alloc.h> 
#include <spdlog/spdlog.h>

namespace ix
{
    VulkanImage::VulkanImage(VulkanContext& context, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage)
        : m_context(context)
        , m_format(format)
        , m_extent(extent)
        , m_isBorrowed(false)
    {
        VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.extent = { extent.width, extent.height, 1 };
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.format = format;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage = usage;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if (vmaCreateImage(m_context.getAllocator(), &imgInfo, &allocInfo, &m_handle, &m_allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("VulkanImage: Failed to create image via VMA!");
        }

        createView();
    }

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
        if (m_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_context.device(), m_view, nullptr);
            m_view = VK_NULL_HANDLE;
        }

        if (!m_isBorrowed && m_handle != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context.getAllocator(), m_handle, m_allocation);

            m_handle = VK_NULL_HANDLE;
            m_allocation = nullptr;
        }
    }

    void VulkanImage::createView()
    {
        // Determine if this is a Depth or Color image based on format
        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        if (m_format == VK_FORMAT_D32_SFLOAT || m_format == VK_FORMAT_D32_SFLOAT_S8_UINT || m_format == VK_FORMAT_D24_UNORM_S8_UINT) {
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = m_handle;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

        if (vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS) {
            throw std::runtime_error("VulkanImage: Failed to create image view!");
        }
    }

    void VulkanImage::transition(VkCommandBuffer cmd, VkImageLayout newLayout) {
        if (m_currentLayout == newLayout) return;

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.oldLayout = m_currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_handle;

        // Determine Aspect
        bool isDepth = (m_format == VK_FORMAT_D32_SFLOAT ||
            m_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            m_format == VK_FORMAT_D24_UNORM_S8_UINT);

        barrier.subresourceRange = {
            static_cast<VkImageAspectFlags>(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            0, 1, 0, 1
        };

        // Map Layouts to Stages and Access Masks
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcAccessMask = isDepth ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_NONE;
            barrier.srcStageMask = isDepth ? VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
        m_currentLayout = newLayout;
    }

    void VulkanImage::copyFromBuffer(VkCommandBuffer cmd, VkBuffer buffer) const
    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { m_extent.width, m_extent.height, 1 };

        vkCmdCopyBufferToImage(cmd, buffer, m_handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void VulkanImage::uploadData(void* pixels, uint32_t size) 
    {
        VulkanBuffer staging(m_context, size, 1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        staging.map();
        staging.writeToBuffer(pixels);
        staging.unmap();

        m_context.immediateSubmit([&](VkCommandBuffer cmd) {
            transition(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            copyFromBuffer(cmd, staging.getBuffer());
            transition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });
    }


    VkDescriptorImageInfo VulkanImage::getImageInfo(VkSampler sampler, VkImageLayout layout)
    {
        return VkDescriptorImageInfo{ sampler, m_view, layout };
    }

    VkDescriptorImageInfo VulkanImage::getDescriptorInfo(VkSampler sampler) const {
        VkDescriptorImageInfo info{};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_view;
        info.sampler = sampler;
        return info;
    }
}