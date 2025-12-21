#include "common/engine_pch.h"
#include "vk_context.h"
#include <set>
#include <GLFW/glfw3.h>


namespace ix {

    VulkanContext::VulkanContext(VulkanInstance& instance, Window_I& window)
        : m_window(window) {

        if (glfwCreateWindowSurface(instance.get(), (GLFWwindow*)window.getNativeHandle(), nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create window surface!");
        }

        pickPhysicalDevice(instance.get());
        checkCapabilities();
        createLogicalDevice();
    }

    VulkanContext::~VulkanContext() {
        vkDestroyDevice(m_logicalDevice, nullptr);
    }

    void VulkanContext::pickPhysicalDevice(VkInstance instance) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            m_physicalDevice = device;
            break;
        }
    }

    void VulkanContext::checkCapabilities() {
        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &features13;

        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

        m_capabilities.hasDynamicRendering = features13.dynamicRendering;
        m_capabilities.hasBindlessIndexing = true;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        m_capabilities.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    }

    void VulkanContext::createLogicalDevice() {

        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = m_capabilities.hasDynamicRendering;
        features13.synchronization2 = VK_TRUE;

        VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.pNext = &features13;

        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create logical device!");
        }

        vkGetDeviceQueue(m_logicalDevice, 0, 0, &m_graphicsQueue);
    }
}