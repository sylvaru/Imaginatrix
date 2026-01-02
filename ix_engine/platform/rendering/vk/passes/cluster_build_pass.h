// cluster_build_pass.h
#pragma once
#include "render_graph_pass_i.h"

namespace ix
{
    class VulkanComputePipeline;


    class ClusterBuildPass : public RenderGraphPass_I
    {
    public:
        ClusterBuildPass(const std::string& name);
        virtual ~ClusterBuildPass() = default;
        PassType getPassType() const override { return PassType::Compute; }

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;

        void forceRebuild() { m_needsUpdate = true; }

    private:
        VulkanComputePipeline* m_cachedPipeline = nullptr;
        bool m_needsUpdate = true;
    };
}