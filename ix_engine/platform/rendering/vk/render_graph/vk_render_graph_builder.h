// vk_render_graph_builder.h
#pragma once
#include <string>
#include <vector>
#include "global_common/ix_global_pods.h"
#include "render_graph_pass_i.h"

namespace ix 
{
    class RenderGraphRegistry;
    class VulkanImage;

    enum class AccessType { 
        Read, 
        Write 
    };

    struct ResourceRequest {
        std::string name;
        AccessType access;
        PassType passType;
    };

    class RenderGraphBuilder 
    {
    public:
        RenderGraphBuilder(
            RenderGraphRegistry& registry,
            std::vector<ResourceRequest>& passRequests,
            const RenderGraphCompileConfig& config,
            PassType passType);

        void write(const std::string& name);
        void read(const std::string& name);
        void importExternalImage(const std::string& name, VulkanImage* image);
        VulkanPipelineManager* getPipelineManager() const { return m_config.pipelineManager; }
        VulkanContext* getContext() const { return m_config.context; }

    private:
        RenderGraphRegistry& m_registry;
        std::vector<ResourceRequest>& m_currentPassRequests;
        const RenderGraphCompileConfig& m_config;
        PassType m_passType;
    };
}