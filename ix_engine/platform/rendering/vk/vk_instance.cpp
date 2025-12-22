// vk_instance.cpp
#include "common/engine_pch.h"
#include "vk_instance.h"
#include <GLFW/glfw3.h>

namespace ix {

    VulkanInstance::VulkanInstance(const char* appName) {
#ifdef NDEBUG
        m_enableValidation = false;
#endif
        createInstance(appName);
        if (m_enableValidation) {
            setupDebugMessenger();
        }
    }

    VulkanInstance::~VulkanInstance() {
        if (m_debugMessenger != VK_NULL_HANDLE) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) func(m_instance, m_debugMessenger, nullptr);
        }
        vkDestroyInstance(m_instance, nullptr);
    }

    void VulkanInstance::createInstance(const char* appName) {
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = appName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Imaginatrix";
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // Extensions
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (m_enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        if (m_enableValidation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create instance!");
        }
    }

    // Static callback for debug messages
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            spdlog::warn("Vulkan Validation: {}", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    void VulkanInstance::setupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func) func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    }
}