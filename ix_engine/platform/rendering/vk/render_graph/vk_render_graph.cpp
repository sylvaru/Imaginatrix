// vk_render_graph.cpp
#include "common/engine_pch.h"
#include "vk_render_graph.h"
#include "platform/rendering/vk/vk_image.h"
#include "global_common/ix_gpu_types.h"

namespace ix 
{
    void RenderGraph::addPass(std::unique_ptr<RenderGraphPass_I> pass) 
    
    {
        m_compiledPasses.push_back({ std::move(pass), {} });
    }

    void RenderGraph::importImage(const std::string& name, VulkanImage* image) 
    {
        m_registry.registerExternalImage(name, image);
    }

    void RenderGraph::compile() 
    {
        for (auto& entry : m_compiledPasses) 
        {
            entry.requests.clear();
            RenderGraphBuilder builder(m_registry, entry.requests);
            entry.pass->setup(builder);
        }
    }

    void RenderGraph::execute(const FrameContext& ctx) 
    {
        for (auto& entry : m_compiledPasses) 
        {
    
            for (const auto& request : entry.requests) 
            {
                transitionResource(ctx.commandBuffer, request);
            }

            entry.pass->execute(ctx, m_registry);
        }
    }

    void RenderGraph::transitionResource(VkCommandBuffer cmd, const ResourceRequest& request) 
    {
        auto& state = m_registry.getResourceState(request.name);

        if (!state.physicalImage) 
        {
            spdlog::error("RenderGraph: Cannot transition '{}' - no image bound!", request.name);
            return;
        }

        VulkanImage* img = state.physicalImage;

        VkImageLayout targetLayout;

        if (request.name == "DepthBuffer") 
        {
            targetLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        }
        else if (request.access == AccessType::Write) 
        {
            targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        else 
        {
            targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (img->getLayout() != targetLayout) 
        {
            img->transition(cmd, targetLayout);

            state.currentLayout = targetLayout;

        }
    }
}