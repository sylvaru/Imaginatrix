// vk_image.h
#pragma once
#include <vulkan/vulkan.h>


struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;

namespace ix 
{
    class VulkanContext;
    class VulkanBuffer;

    class VulkanImage 
    {
    public:
        VulkanImage(VulkanContext& context,
            VkExtent2D extent,
            VkFormat format,
            VkImageUsageFlags usage,
            uint32_t layerCount = 1,
            bool createCube = false);

        VulkanImage(VulkanContext& context, VkImage handle, VkFormat format, VkExtent2D extent);

        ~VulkanImage();

        void transition(VkCommandBuffer cmd, VkImageLayout newLayout);
        void copyFromBuffer(VkCommandBuffer cmd, VkBuffer buffer) const;
        void uploadData(void* pixels, uint32_t size);
        VkImageView createAdditionalView(VkImageViewType type, uint32_t layerCount);

        // Getters
        VkImage getHandle() const { return m_handle; }
        VkImageView getView() const { return m_view; }
        VkFormat getFormat() const { return m_format; }
        VkExtent2D getExtent() const { return m_extent; }
        VkImageLayout getLayout() const { return m_currentLayout; }
        VkDescriptorImageInfo getImageInfo(VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);
        VkDescriptorImageInfo getDescriptorInfo(VkSampler sampler) const;
        bool isDepthFormat() const;

    private:
        void createView();

        VulkanContext& m_context;
        VkImage m_handle = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        std::vector<VkImageView> m_additionalViews;
        VmaAllocation m_allocation = VK_NULL_HANDLE;

        VkFormat m_format;
        VkExtent2D m_extent;
        volatile VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool m_isBorrowed = false;
        uint32_t m_layerCount = 1;
        bool m_isCube = false;


    };
}