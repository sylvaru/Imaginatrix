// ComputeCullingPass.h
#pragma once
#include "render_graph_pass_i.h"

namespace ix 
{
    class VulkanComputePipeline;


    class ComputeCullingPass : public RenderGraphPass_I 
    {
    public:
        ComputeCullingPass(const std::string& name);
        virtual ~ComputeCullingPass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;

    private:
        VulkanComputePipeline* m_cachedPipeline = nullptr;
    };
}