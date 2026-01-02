#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix
{
    class VulkanPipeline;

    class DepthPrePass : public RenderGraphPass_I
    {
    public:
        DepthPrePass(const std::string& name);
        virtual ~DepthPrePass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;


    private:
        VulkanPipeline* m_cachedPipeline = nullptr;
    };
}

