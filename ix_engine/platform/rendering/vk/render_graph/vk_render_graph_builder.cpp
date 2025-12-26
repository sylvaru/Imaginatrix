#include "common/engine_pch.h"
#include "vk_render_graph_builder.h"
#include "vk_render_graph_registry.h"

namespace ix 
{
    RenderGraphBuilder::RenderGraphBuilder(
        RenderGraphRegistry& registry,
        std::vector<ResourceRequest>& passRequests,
        const RenderGraphCompileConfig& config)
        : m_registry(registry)
        , m_currentPassRequests(passRequests) 
        , m_config(config)
    {
    }

    void RenderGraphBuilder::write(const std::string& name) 
    {
        m_currentPassRequests.push_back({ name, AccessType::Write });
    }

    void RenderGraphBuilder::read(const std::string& name) 
    {
        m_currentPassRequests.push_back({ name, AccessType::Read });
    }

    void RenderGraphBuilder::importExternalImage(const std::string& name, VulkanImage* image) 
    {
        m_registry.registerExternalImage(name, image);
    }
}