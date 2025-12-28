// compute_culling_pass.cpp
#include "common/engine_pch.h"
#include "compute_culling_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_descriptor_manager.h"
#include "platform/rendering/vk/vk_buffer.h"

namespace ix
{
    ComputeCullingPass::ComputeCullingPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ComputeCullingPass::setup(RenderGraphBuilder& builder) 
    {
  
        builder.read("InstanceDb"); // Read from the full database

        builder.write("CulledInstances"); // Write to the filtered list

        m_cachedPipeline = builder.getPipelineManager()->getComputePipeline("FrustumCull");
    }

    void ComputeCullingPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        VkCommandBuffer cmd = state.frame.commandBuffer;

        VulkanBuffer* culledOut = registry.getBuffer("CulledInstances");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cachedPipeline->getHandle());

        // Bind Pre-Baked Sets (Sets 0, 1, and 2)
        VkDescriptorSet sets[] = 
        {
            state.frame.globalDescriptorSet,
            state.frame.bindlessDescriptorSet,
            state.frame.cullingDescriptorSet  
        };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_cachedPipeline->getLayout(), 0, 3, sets, 0, nullptr);

        // Push Constants
        CullingPushConstants pcs;
        pcs.viewProj = state.view.projectionMatrix * state.view.viewMatrix;
        pcs.maxInstances = state.frame.instanceCount;
        pcs.debugCulling = false; // toggle debug culling

        vkCmdPushConstants(cmd, m_cachedPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(CullingPushConstants), &pcs);

        // Dispatch
        uint32_t groupCount = (state.frame.instanceCount + 63) / 64;
        vkCmdDispatch(cmd, groupCount, 1, 1);

        // Barrier
        VkBufferMemoryBarrier barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.buffer = culledOut->getBuffer();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);
    }
}