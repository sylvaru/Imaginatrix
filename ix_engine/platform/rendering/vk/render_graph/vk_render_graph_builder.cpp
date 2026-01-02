#include "common/engine_pch.h"
#include "vk_render_graph_builder.h"
#include "vk_render_graph_registry.h"

namespace ix 
{
    RenderGraphBuilder::RenderGraphBuilder(
        RenderGraphRegistry& registry,
        std::vector<ResourceRequest>& passRequests,
        const RenderGraphCompileConfig& config,
        PassType passType)
        : m_registry(registry)
        , m_currentPassRequests(passRequests)
        , m_config(config)
        , m_passType(passType)
    {
    }

    void RenderGraphBuilder::write(const std::string& name) 
    {
        m_currentPassRequests.emplace_back(name, AccessType::Write, m_passType);
    }

    void RenderGraphBuilder::read(const std::string& name) 
    {
        m_currentPassRequests.emplace_back(name, AccessType::Read, m_passType);
    }

    void RenderGraphBuilder::importExternalImage(const std::string& name, VulkanImage* image) 
    {
        m_registry.registerExternalImage(name, image);
    }
}