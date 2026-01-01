#include "common/engine_pch.h"
#include "vk_context.h"
#include "vk_instance.h"
#include "window_i.h"

#include <set>
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>


namespace ix 
{

    VulkanContext::VulkanContext(VulkanInstance& instance, Window_I& window)
        : m_window(window) 
        , m_instanceHandle(instance.get())
    {

        createSurface(instance.get());
        pickPhysicalDevice(instance.get());
        checkCapabilities();
        createLogicalDevice();
        createAllocator(instance.get());
        createImmCommandPool();
    }

    VulkanContext::~VulkanContext()
    {
        if (m_immCommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(m_logicalDevice, m_immCommandPool, nullptr);
        if (m_allocator) vmaDestroyAllocator(m_allocator);
        if (m_logicalDevice) vkDestroyDevice(m_logicalDevice, nullptr);
        if (m_surface) vkDestroySurfaceKHR(m_instanceHandle, m_surface, nullptr);
    }

    void VulkanContext::createSurface(VkInstance instance) 
    {
        GLFWwindow* glfwWindow =
            static_cast<GLFWwindow*>(m_window.getNativeHandle());

        glfwCreateWindowSurface(
            instance,
            glfwWindow,
            nullptr,
            &m_surface
        );
    }
    void VulkanContext::createAllocator(VkInstance instance)
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_physicalDevice;
        allocatorInfo.device = m_logicalDevice;
        allocatorInfo.instance = instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
            throw std::runtime_error("VulkanContext: Failed to create VMA allocator!");
        }
    }

    void VulkanContext::pickPhysicalDevice(VkInstance instance) 
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            spdlog::error("Vulkan: Failed to find GPUs with Vulkan support!");
            throw std::runtime_error("Vulkan: Failed to find GPUs with Vulkan support!");
        }

        spdlog::info("Vulkan: {} physical devices found", deviceCount);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) 
        {
            if (isDeviceSuitable(device)) 
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE) {
            spdlog::critical("Vulkan: Failed to find a suitable GPU! Check your 'isDeviceSuitable' requirements.");
            throw std::runtime_error("Vulkan: Failed to find a suitable GPU!");
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        spdlog::info("Vulkan: Selected Physical Device: [{}]", props.deviceName);
    }

    void VulkanContext::checkCapabilities() 
    {
        // Feature structures
        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceDescriptorIndexingFeatures indexing{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

        // Build pNext chain to query everything at once
        features2.pNext = &indexing;
        indexing.pNext = &features13;

        if (m_physicalDevice == VK_NULL_HANDLE) return;

        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

        // Check capabilities
        m_capabilities.hasDynamicRendering = (features13.dynamicRendering == VK_TRUE);

        m_capabilities.hasMultiDrawIndirect = (features2.features.multiDrawIndirect == VK_TRUE);

        m_capabilities.hasBindlessIndexing = (indexing.runtimeDescriptorArray == VK_TRUE &&
            indexing.descriptorBindingPartiallyBound == VK_TRUE);

        // Check limits
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        m_capabilities.maxAnisotropy = props.limits.maxSamplerAnisotropy;

        // Log the results
        spdlog::info("Vulkan Capabilities Check:");
        spdlog::info("  Dynamic Rendering: {}", m_capabilities.hasDynamicRendering);
        spdlog::info("  Multi-Draw Indirect: {}", m_capabilities.hasMultiDrawIndirect);
        spdlog::info("  Bindless Indexing: {}", m_capabilities.hasBindlessIndexing);
    }

    void VulkanContext::createLogicalDevice() 
    {
        m_queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { m_queueFamilyIndices.graphicsFamily, m_queueFamilyIndices.presentFamily };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            qci.queueFamilyIndex = queueFamily;
            qci.queueCount = 1;
            qci.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(qci);
        }

        // Feature 1.3 Features
        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = m_capabilities.hasDynamicRendering;
        features13.synchronization2 = VK_TRUE;

        // Enable Descriptor Indexing (Bindless)
        VkPhysicalDeviceDescriptorIndexingFeatures indexing{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        indexing.pNext = &features13;
        if (m_capabilities.hasBindlessIndexing) {
            indexing.runtimeDescriptorArray = VK_TRUE;
            indexing.descriptorBindingPartiallyBound = VK_TRUE;
            indexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        }
      

        // Enable Core Features
        VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures.pNext = &indexing;
        deviceFeatures.features.samplerAnisotropy = VK_TRUE;
        deviceFeatures.features.multiDrawIndirect = m_capabilities.hasMultiDrawIndirect;

        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.pNext = &deviceFeatures;
        dci.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        dci.pQueueCreateInfos = queueCreateInfos.data();

        auto enabledExtensions = getEnabledExtensions(m_physicalDevice);
        dci.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        dci.ppEnabledExtensionNames = enabledExtensions.data();

        if (vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_logicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create logical device! Check supported features.");
        }

        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
    }

    QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const
    {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                indices.graphicsFamilyHasValue = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
                indices.presentFamilyHasValue = true; 
            }

            if (indices.isComplete()) break;
        }
        return indices;
    }

    bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) 
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;

        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        // Logging the suitability check
        spdlog::debug("Checking suitability for: {}", props.deviceName);

        if (!indices.isComplete())
            spdlog::warn("  - Device [{}] rejected: Missing required queue families.", props.deviceName);

        if (!extensionsSupported)
            spdlog::warn("  - Device [{}] rejected: Missing required extensions.", props.deviceName);

        if (!swapChainAdequate)
            spdlog::warn("  - Device [{}] rejected: Swapchain support is inadequate.", props.deviceName);

        if (!supportedFeatures.samplerAnisotropy)
            spdlog::warn("  - Device [{}] rejected: Sampler Anisotropy not supported.", props.deviceName);

        return indices.isComplete() &&
            extensionsSupported &&
            swapChainAdequate &&
            supportedFeatures.samplerAnisotropy;
    }


    SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }

    bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) 
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    std::vector<const char*> VulkanContext::getEnabledExtensions(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::vector<const char*> activeExtensions;

        // Get Device Properties to check Vulkan Version
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        uint32_t major = VK_API_VERSION_MAJOR(props.apiVersion);
        uint32_t minor = VK_API_VERSION_MINOR(props.apiVersion);

        for (const char* extName : m_deviceExtensions) {
            // Check if the extension is available on this hardware
            bool found = false;
            for (const auto& available : availableExtensions) {
                if (strcmp(extName, available.extensionName) == 0) {
                    found = true;
                    break;
                }
            }

            if (found) {
                // Skip enabling extensions that are already Core in 1.3
                if (major == 1 && minor >= 3) {
                    if (strcmp(extName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0 ||
                        strcmp(extName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0 ||
                        strcmp(extName, VK_KHR_MAINTENANCE3_EXTENSION_NAME) == 0)
                    {
                        spdlog::debug("Extension {} is core in 1.3, skipping explicit enable.", extName);
                        continue;
                    }
                }
                activeExtensions.push_back(extName);
            }
            else {
                spdlog::error("Required extension NOT FOUND: {}", extName);
            }
        }

        return activeExtensions;
    }

    void VulkanContext::createImmCommandPool() 
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Optimization for short-lived buffers

        if (vkCreateCommandPool(m_logicalDevice, &poolInfo, nullptr, &m_immCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create immediate command pool!");
        }
    }

    void VulkanContext::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& func) const
    {
        if (!this) {
            spdlog::error("VulkanContext::immediateSubmit called on NULL or invalid context!");
            return;
        }

        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_immCommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        if (vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, &cmd) != VK_SUCCESS) {
            spdlog::error("Failed to allocate immediate command buffer!");
            return;
        }

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);
        func(cmd);
        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence;
        vkCreateFence(m_logicalDevice, &fenceInfo, nullptr, &fence);

        VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence);

        // Wait for the fence (more robust than QueueWaitIdle)
        vkWaitForFences(m_logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);

        vkDestroyFence(m_logicalDevice, fence, nullptr);
        vkFreeCommandBuffers(m_logicalDevice, m_immCommandPool, 1, &cmd);
    }
}