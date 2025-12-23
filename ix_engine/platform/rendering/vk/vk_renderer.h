// vk_renderer.h
#pragma once
#include "renderer_i.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <nlohmann/json.hpp>

#include "global_common/ix_gpu_types.h"
#include "vk_buffer.h"

namespace ix 
{
    class VulkanInstance;
	class VulkanContext;
	class VulkanSwapchain;
	class VulkanPipelineManager; 
    class VulkanDescriptorManager;
    class RenderGraph;
    class VulkanImage;


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
        void waitIdle() override;
        bool beginFrame(FrameContext& ctx) override;
        void endFrame(const FrameContext& ctx) override;
        void loadPipelines(const nlohmann::json& json) override;
        void render(const FrameContext& ctx) override;

        // Misc
        void updateGlobalUbo(const FrameContext& ctx);

        // Getters
        void* getAPIContext() override { return m_context.get(); }
        FrameData& getCurrentFrame() { return m_frames[m_currentFrameIndex]; }
        RenderExtent getSwapchainExtent() const override;
        VulkanSwapchain* getSwapchain() override { return m_swapchain.get(); }
        uint32_t getCurrentImageIndex() const override { return m_currentImageIndex; }
        VulkanPipelineManager* getPipelineManager() const { return m_pipelineManager.get(); }
      
    private:
        // Internal Helpers
        void recreateSwapchain();
        void createCommandBuffers();
        void createSyncObjects();

        void createGlobalLayout();

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
        std::unique_ptr<VulkanDescriptorManager> m_descriptorManager;
        
        VkPipelineLayout m_defaultLayout;
        VkDescriptorSetLayout m_globalDescriptorLayout;

        std::vector<std::unique_ptr<VulkanBuffer>> m_globalUboBuffers;
       
        GlobalUbo m_globalUboData{};

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::unique_ptr<VulkanImage> m_depthImage;



	};
}