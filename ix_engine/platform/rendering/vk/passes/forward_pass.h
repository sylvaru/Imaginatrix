#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix 
{
    class ForwardPass : public RenderGraphPass_I 
    {
    public:
        ForwardPass(const std::string& name);
        virtual ~ForwardPass() = default;

        // Declares that we write to the "Backbuffer"
        void setup(RenderGraphBuilder& builder) override;

        // Performs the actual draw calls
        void execute(const FrameContext& ctx, RenderGraphRegistry& registry) override;


    };
}