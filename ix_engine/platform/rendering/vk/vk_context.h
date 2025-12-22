// vk_context.h
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace ix
{
    class VulkanInstance;
    struct Window_I;
}
namespace ix
{

    struct VulkanCapabilities {
        bool hasDynamicRendering = false;
        bool hasBindlessIndexing = false;
        bool hasRayTracing = false;
        float maxAnisotropy = 1.0f;
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct QueueFamilyIndices {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    class VulkanContext {
    public:
        VulkanContext(VulkanInstance& instance, Window_I& window);
        ~VulkanContext();

        // Getters
        VkDevice device() const { return m_logicalDevice; }
        VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
        VkSurfaceKHR surface() const { return m_surface; }
        VkQueue graphicsQueue() const { return m_graphicsQueue; }
        const VulkanCapabilities& getCaps() const { return m_capabilities; }

        bool useDynamicRendering() const { return m_capabilities.hasDynamicRendering; }

        uint32_t getGraphicsFamily() const { return m_queueFamilyIndices.graphicsFamily; }
        VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
        VkQueue getPresentQueue() const { return m_presentQueue; }

    private:
        void createSurface(VkInstance instance);
        void pickPhysicalDevice(VkInstance instance);
        void createLogicalDevice();
        void checkCapabilities();

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        bool isDeviceSuitable(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    private:
        Window_I& m_window;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        VkInstance m_instanceHandle;

        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_logicalDevice = VK_NULL_HANDLE;

        VulkanCapabilities m_capabilities;
        QueueFamilyIndices m_queueFamilyIndices;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;


        const std::vector<const char*> m_deviceExtensions =
        { 
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_MAINTENANCE1_EXTENSION_NAME,
            VK_KHR_MAINTENANCE3_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
        };

    };
}