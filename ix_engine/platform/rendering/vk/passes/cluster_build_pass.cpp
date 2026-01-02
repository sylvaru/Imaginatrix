// cluster_build_pass.cpp
#include "common/engine_pch.h"
#include "cluster_build_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_descriptor_manager.h"
#include "platform/rendering/vk/vk_buffer.h"

namespace ix
{
    ClusterBuildPass::ClusterBuildPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ClusterBuildPass::setup(RenderGraphBuilder& builder)
    {
        builder.write("ClusterAABBbuffer");
        m_cachedPipeline = builder.getPipelineManager()->getComputePipeline("ClusterBuild");
    }

    void ClusterBuildPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        VkCommandBuffer cmd = state.frame.commandBuffer;
        if (!m_needsUpdate) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cachedPipeline->getHandle());

        ClusterConstants push{};
        push.invProjection = glm::inverse(state.view.clusterProjection);
        push.zNear = 0.1f;
        push.zFar = 1000.0f;
        push.gridX = 16;
        push.gridY = 9;
        push.gridZ = 24;

        vkCmdPushConstants(cmd, m_cachedPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cachedPipeline->getLayout(), 0, 1, &state.frame.globalDescriptorSet, 0, nullptr);

        // Dispatch: X=16, Y=9, Z=24 (one thread per cluster)
        vkCmdDispatch(cmd, 16, 9, 24);

        m_needsUpdate = false;
    }
}