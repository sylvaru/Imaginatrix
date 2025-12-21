#pragma once
#include "renderer_i.h"
#include "vk_context.h"

#include <vulkan/vulkan.h>


namespace ix 
{
	class VulkanContext;
	class VulkanSwapchain;
	class VulkanPipeline; 
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

    /*
        The VulkanRenderer will own the VulkanInstance first, then use it to initialize the VulkanContext.
    */
	
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

    private:
        // Internal Helpers
        void recreateSwapchain();
        void createCommandBuffers();
        void createSyncObjects();

    private:
        Window_I& m_window;

        std::unique_ptr<VulkanInstance> m_instance;
        std::unique_ptr<VulkanContext> m_context;

        VkCommandPool m_commandPool = VK_NULL_HANDLE;

	};
}