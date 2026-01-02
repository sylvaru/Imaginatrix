// cluster_culling_pass.h
#pragma once
#include "render_graph_pass_i.h"

namespace ix
{
    class VulkanComputePipeline;


    class ClusterCullingPass : public RenderGraphPass_I
    {
    public:
        ClusterCullingPass(const std::string& name);
        virtual ~ClusterCullingPass() = default;
        PassType getPassType() const override { return PassType::Compute; }

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;

    private:
        VulkanComputePipeline* m_cachedPipeline = nullptr;
    };
}