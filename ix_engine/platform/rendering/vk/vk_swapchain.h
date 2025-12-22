// vk_swapchain.h
#pragma once
#include <vulkan/vulkan.h>
#include <memory> 

namespace ix
{
    class VulkanImage;
    class VulkanContext;
}

namespace ix
{

    struct SwapchainImage {
        std::unique_ptr<VulkanImage> image;
        VkSemaphore renderFinishedSemaphore;
    };

    class VulkanSwapchain {
    public:
        VulkanSwapchain(VulkanContext& context, VkExtent2D extent, VulkanSwapchain* old = nullptr);
        ~VulkanSwapchain();

        // Delete copy/move
        VulkanSwapchain(const VulkanSwapchain&) = delete;
        void operator=(const VulkanSwapchain&) = delete;

        // Getters
        VkSwapchainKHR get() const { return m_swapchain; }
        VkFormat getFormat() const { return m_imageFormat; }
        VkExtent2D getExtent() const { return m_extent; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
        VkImage getImage(uint32_t index) const { return m_images[index]; }
        VulkanImage* getImageWrapper(uint32_t index) { return m_frames[index].image.get(); }
        VkSemaphore getRenderSemaphore(uint32_t index) { return m_frames[index].renderFinishedSemaphore; }

        // Misc
        VkResult acquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex);

    private:
        void create(VkSwapchainKHR oldHandle);

        // Selection helpers
        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

    private:
        VulkanContext& m_context;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        std::shared_ptr<VulkanSwapchain> m_oldSwapChain;
        VkExtent2D m_extent;
        VkFormat m_imageFormat;

        std::vector<VkImage> m_images;
        std::vector<SwapchainImage> m_frames;
    };
}