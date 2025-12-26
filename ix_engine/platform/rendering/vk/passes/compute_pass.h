// compute_pass.h
#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix
{
    class ComputePass : public RenderGraphPass_I
    {
    public:
        ComputePass(const std::string& name);
        virtual ~ComputePass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const FrameContext& ctx, RenderGraphRegistry& registry) override;

    };
}