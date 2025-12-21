// vk_context.h
#pragma once
#include <vulkan/vulkan.h>
#include "vk_instance.h"
#include "window_i.h"


namespace ix
{


    struct VulkanCapabilities {
        bool hasDynamicRendering = false;
        bool hasBindlessIndexing = false;
        bool hasRayTracing = false;
        float maxAnisotropy = 1.0f;
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

    private:
        void pickPhysicalDevice(VkInstance instance);
        void createLogicalDevice();
        void checkCapabilities();

    private:
        Window_I& m_window;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_logicalDevice = VK_NULL_HANDLE;

        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;

        VulkanCapabilities m_capabilities;
    };
}