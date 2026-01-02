// vk_render_graph.h
#pragma once
#include "render_graph_pass_i.h"
#include "vk_render_graph_registry.h"
#include "vk_render_graph_builder.h"
#include <memory>
#include <vector>

namespace ix 
{
    struct RenderGraphCompileConfig;
    struct RenderState;

    struct PassEntry 
    {
        std::unique_ptr<RenderGraphPass_I> pass;
        std::vector<ResourceRequest> requests;
    };

    class RenderGraph 
    {
    public:
        void addPass(std::unique_ptr<RenderGraphPass_I> pass);
        void compile(const RenderGraphCompileConfig& config);
        void execute(const RenderState& state);

        // Helper to get swapchain into the graph
        void importImage(const std::string& name, VulkanImage* image);
        void importBuffer(const std::string& name, VulkanBuffer* buffer) 
        {
            m_registry.registerExternalBuffer(name, buffer);
        }

        // Get a pass by its name
        RenderGraphPass_I* getPass(const std::string& name) const;

        void clearExternalResources() { m_registry.clearExternalResources(); }
    private:
        void transitionResource(VkCommandBuffer cmd, const ResourceRequest& request);

        RenderGraphRegistry m_registry;
        std::vector<PassEntry> m_compiledPasses;
    };
}