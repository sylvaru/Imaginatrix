// vk_renderer.h
#pragma once
#include "renderer_i.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <nlohmann/json.hpp>

namespace ix 
{
    class VulkanInstance;
	class VulkanContext;
	class VulkanSwapchain;
	class VulkanPipelineManager; 
}

namespace ix
{

    struct DrawCommand 
    {
        uint32_t pipelineID;
        uint32_t materialID;
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        uint32_t indexCount;
        glm::mat4 transform;
    };

    struct FrameData
    {
        VkSemaphore imageAvailableSemapohore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };

	
	class VulkanRenderer : public Renderer_I 
	{
	public:
        VulkanRenderer(Window_I& window);
        ~VulkanRenderer() override;

        // Interface
        void init() override;
        void shutdown() override;
        void onResize(uint32_t width, uint32_t height) override;

        void beginFrame() override;
        void endFrame() override;

        void submitScene(Scene& scene, float alpha) override;

        void loadPipelines(const nlohmann::json& json) override;

        // Getters
        FrameData& getCurrentFrame() { return m_frames[m_currentFrameIndex]; }

    private:
        // Internal Helpers
        void recreateSwapchain();
        void createCommandBuffers();
        void createSyncObjects();

        void createDefaultLayout();

    private:
        Window_I& m_window;

        std::unique_ptr<VulkanInstance> m_instance;
        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<VulkanSwapchain> m_swapchain;

        uint32_t m_currentImageIndex = 0;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;

        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        FrameData m_frames[MAX_FRAMES_IN_FLIGHT]{};
        int m_currentFrameIndex = 0;

        std::unique_ptr<VulkanPipelineManager> m_pipelineManager;
        
        VkPipelineLayout m_defaultLayout;
        VkDescriptorSetLayout m_globalDescriptorLayout;

	};
}