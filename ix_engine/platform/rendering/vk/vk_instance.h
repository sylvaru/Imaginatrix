#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace ix {
    class VulkanInstance {
    public:
        VulkanInstance(const char* appName);
        ~VulkanInstance();

        VkInstance get() const { return m_instance; }
        bool validationEnabled() const { return m_enableValidation; }

    private:
        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        bool m_enableValidation = true;

        void createInstance(const char* appName);
        void setupDebugMessenger();
    };
}