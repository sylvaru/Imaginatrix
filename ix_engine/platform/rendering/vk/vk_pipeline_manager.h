// vk_pipeline_manager.h
#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include "vk_pipeline.h"


namespace ix
{
	class VulkanContext;


    struct PipelineHasher
    {
        size_t operator() (const PipelineState& s) const;
    };

    struct PipelineDefinition {
        std::string name;
        std::string vertPath;
        std::string fragPath;
        PipelineState baseState;
        VkPipelineLayout layout;
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

        void reloadPipelines();

        void clearCache();

        // Getters
        VulkanPipeline* getGraphicsPipeline(PipelineState requestedState); // Get by state "Preffered"
        VulkanPipeline* getGraphicsPipeline(const std::string& name) const; // Get by name

    private:

        VulkanPipeline* bakePipeline(const PipelineDefinition& def);

        VulkanContext& m_context;
        std::vector<PipelineDefinition> m_definitions;
        std::unordered_map<PipelineState, std::shared_ptr<VulkanPipeline>, PipelineHasher> m_stateCache; // Pipeline state cache
        std::unordered_map<std::string, std::shared_ptr<VulkanPipeline>> m_namedCache; // Pipeline name cache

    };
}