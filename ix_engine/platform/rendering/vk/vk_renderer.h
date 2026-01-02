// vk_renderer.h
#pragma once
#include "renderer_i.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <nlohmann/json_fwd.hpp>    

#include "global_common/ix_global_pods.h"
#include "global_common/ix_event_pods.h"
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
    
    struct LightData;


	
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
        bool beginFrame(FrameContext& ctx, const SceneView& view) override;
        void endFrame(const FrameContext& ctx) override;
        void loadPipelines(const nlohmann::json& json) override;
        void compileRenderGraph() override;
        void render(const FrameContext& ctx, const SceneView& view) override;
        void setupImGui() override;

        // Misc
        void updateGlobalUbo(const SceneView& view);
        void updateBindlessTextures(uint32_t slot, VkDescriptorImageInfo& imageInfo);
        void updateBindlessTextures(const std::vector<BindlessUpdateRequest>& updates);
        void setVsync(bool enabled) 
        {
            if (m_vsync != enabled) 
            {
                m_vsync = enabled;
                m_needsSwapchainRecreation = true;
            }
        }
        bool isVsyncEnabled() const { return m_vsync; }

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
        void createDescriptorAndPipelineLayouts();
        
        // Misc
        void updateInstanceBuffer();
        void updateLightBuffer(const SceneView& view);

    private:
        // Core Infrastructure
        Window_I& m_window;
        std::unique_ptr<VulkanInstance> m_instance;
        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<VulkanSwapchain> m_swapchain;
        std::unique_ptr<RenderGraph> m_renderGraph;
        RenderGraphCompileConfig m_renderGraphCompileConfig;

        // Execution & Synchronization
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        FrameData m_frames[MAX_FRAMES_IN_FLIGHT]{};
        int       m_currentFrameIndex = 0;
        uint32_t  m_currentImageIndex = 0;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;

        // Pipeline & Descriptor Management
        std::unique_ptr<VulkanPipelineManager> m_pipelineManager;
        std::vector<std::unique_ptr<VulkanDescriptorManager>> m_descriptorManagers;

        // Layouts
        VkPipelineLayout m_graphicsPipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_computePipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_cullingPipelineLayout = VK_NULL_HANDLE;

        VkDescriptorSetLayout m_globalDescriptorLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_instanceDescriptorLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_cullingDescriptorLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_computeStorageLayout = VK_NULL_HANDLE;

        // Global Resources
        std::unique_ptr<VulkanImage> m_depthImage;
        GlobalUbo m_globalUboData{};
        std::vector<std::unique_ptr<VulkanBuffer>> m_globalUboBuffers;
        std::vector<FrameDescriptorSetGroup>       m_frameDescriptorSets;

        // Bindless System
        VkDescriptorPool      m_bindlessPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_bindlessLayout = VK_NULL_HANDLE;
        VkDescriptorSet       m_bindlessDescriptorSet = VK_NULL_HANDLE;

        // GPU-Driven Rendering & Culling
        std::unique_ptr<VulkanBuffer> m_instanceBuffer;       // Scene Database (Input)
        std::unique_ptr<VulkanBuffer> m_culledInstanceBuffer; // Indirect Commands + Culled Data (Output)
        uint32_t m_currentInstanceCount = 0;

        // CPU-Side Batching & Caches
        std::vector<RenderBatch> m_renderBatches;
        std::vector<GPUInstanceData> m_cpuInstanceCache;
        std::unordered_map<MeshHandle, std::vector<GPUInstanceData>> m_batchMapCache;
        std::vector<std::unique_ptr<VulkanBuffer>> m_lightBuffers;
        std::unique_ptr<VulkanBuffer> m_clusterAABBbuffer;
        std::unique_ptr<VulkanBuffer> m_lightIndexListBuffer;// pool of light IDs
        std::unique_ptr<VulkanBuffer> m_lightGridBuffer;
        std::unique_ptr<LightData> m_cpuLightCache;
        std::unique_ptr<VulkanBuffer> m_atomicCounterBuffer;
        // Imgui
        VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;

        // Misc
        bool m_vsync = true;
        bool m_needsSwapchainRecreation = false;
	};
}