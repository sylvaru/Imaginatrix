// cluster_culling_pass.cpp
#include "common/engine_pch.h"
#include "cluster_culling_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_descriptor_manager.h"
#include "platform/rendering/vk/vk_buffer.h"

namespace ix
{
    ClusterCullingPass::ClusterCullingPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ClusterCullingPass::setup(RenderGraphBuilder& builder)
    {
        // Inputs
        builder.read("ClusterAABBbuffer");

        // Outputs
        builder.write("LightGridBuffer");
        builder.write("LightIndexBuffer");
        builder.write("AtomicCounter");

        m_cachedPipeline = builder.getPipelineManager()->getComputePipeline("ClusterCulling");
    }

    void ClusterCullingPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        VkCommandBuffer cmd = state.frame.commandBuffer;

        // Reset Atomic Counter to 0
        uint32_t zero = 0;
        vkCmdUpdateBuffer(cmd, state.frame.atomicCounterBuffer, 0, sizeof(uint32_t), &zero);

        // Barrier: Transfer -> Compute
        // Ensure the reset is finished before the shader tries to increment it
        VkBufferMemoryBarrier barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.buffer = state.frame.atomicCounterBuffer;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);

        // Dispatch Culling Shader
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cachedPipeline->getHandle());

        // Grid: 16x9x24
        vkCmdDispatch(cmd, 16, 9, 24);
    }
}