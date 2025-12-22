// vk_pipeline_manager.h
#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

namespace ix
{
	class VulkanPipeline;
	class VulkanContext;
    struct PipelineState;
}

namespace ix
{

    struct PipelineHasher
    {
        size_t operator() (const PipelineState& s) const;
    };
    class VulkanPipelineManager {
    public:
        VulkanPipelineManager(VulkanContext& context);
        ~VulkanPipelineManager();

        VulkanPipeline* createGraphicsPipeline(
            const std::string& name,
            const std::string& vertPath,
            const std::string& fragPath,
            const PipelineState& state,
            VkPipelineLayout layout);

        void clearCache();

        // Getters
        VulkanPipeline* getGraphicsPipeline(const PipelineState& state) const; // Get by state "Preffered"
        VulkanPipeline* getGraphicsPipeline(const std::string& name) const; // Get by name

    private:
        VulkanContext& m_context;
        std::unordered_map<PipelineState, std::shared_ptr<VulkanPipeline>, PipelineHasher> m_stateCache; // Pipeline state cache
        std::unordered_map<std::string, std::shared_ptr<VulkanPipeline>> m_namedCache; // Pipeline name cache

    };
}