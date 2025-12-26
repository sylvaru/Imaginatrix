#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix 
{
    class VulkanPipeline;

    class ForwardPass : public RenderGraphPass_I 
    {
    public:
        ForwardPass(const std::string& name);
        virtual ~ForwardPass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const FrameContext& ctx, RenderGraphRegistry& registry) override;


    private:
        VulkanPipeline* m_cachedPipeline = nullptr;
    };
}

