// vk_image.h
#pragma once
#include <vulkan/vulkan.h>


struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;

namespace ix 
{
    class VulkanContext;

    class VulkanImage 
    {
    public:
        VulkanImage(VulkanContext& context, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);

        VulkanImage(VulkanContext& context, VkImage handle, VkFormat format, VkExtent2D extent);

        ~VulkanImage();

        void transition(VkCommandBuffer cmd, VkImageLayout newLayout);

        // Getters
        VkImage getHandle() const { return m_handle; }
        VkImageView getView() const { return m_view; }
        VkFormat getFormat() const { return m_format; }
        VkExtent2D getExtent() const { return m_extent; }
        VkImageLayout getLayout() const { return m_currentLayout; }

    private:
        void createView();

        VulkanContext& m_context;
        VkImage m_handle = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;

        VkFormat m_format;
        VkExtent2D m_extent;
        volatile VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool m_isBorrowed = false;

    };
}