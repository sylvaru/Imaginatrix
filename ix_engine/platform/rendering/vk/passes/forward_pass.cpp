#include "common/engine_pch.h"
#include "forward_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_context.h"
#include "platform/rendering/vk/vk_image.h"

#include "engine.h"
#include "core/scene_manager.h"
#include "core/components.h"
#include "global_common/ix_global_pods.h"


namespace ix
{
    ForwardPass::ForwardPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ForwardPass::setup(RenderGraphBuilder& builder) 
    {
        builder.write("BackBuffer");
        builder.write("DepthBuffer");
        m_cachedPipeline = builder.getPipelineManager()->getGraphicsPipeline("ForwardPass");
    }

    void ForwardPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        if (!m_cachedPipeline || state.frame.instanceCount == 0) return;

        VkCommandBuffer cmd = state.frame.commandBuffer;
        auto& colorState = registry.getResourceState("BackBuffer");
        auto& depthState = registry.getResourceState("DepthBuffer");

        VulkanImage* targetImage = colorState.physicalImage;
        VulkanImage* depthImage = depthState.physicalImage;

        if (!targetImage || !depthImage) return;

        VkExtent2D extent = targetImage->getExtent();

        // Rendering Attachments Setup
        VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.imageView = targetImage->getView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {{ 0.1f, 0.1f, 0.1f, 1.0f }} };

        VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachment.imageView = depthImage->getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, extent };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Set Dynamic States
        VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind Pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedPipeline->getHandle());

        // Bind all 3 sets (Global, Bindless, Instance)
        VkDescriptorSet sets[] = 
        {
           state.frame.globalDescriptorSet,
           state.frame.bindlessDescriptorSet,
           state.frame.instanceDescriptorSet
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_cachedPipeline->getLayout(), 0, 3, sets, 0, nullptr);


        // Log
        static bool debug_log_once = true;
        if (debug_log_once) {
            spdlog::info("--- Forward pass Draw Call Breakdown ---");
            for (const auto& batch : *state.frame.renderBatches) 
            {
                spdlog::info("Batch: MeshHandle {}, Instances: {}, Offset: {}",
                    batch.meshHandle, batch.instanceCount, batch.firstInstance);
            }
            debug_log_once = false; // Only log the first frame
        }

        // The Batch Loop
        for (const auto& batch : *state.frame.renderBatches)
        {
            VulkanMesh* mesh = state.system.assetManager->getMesh(batch.meshHandle);
            if (!mesh) continue;

            VkDeviceSize offsets[] = { 0 };
            VkBuffer vBuffer = mesh->vertexBuffer->getBuffer();
            VkBuffer iBuffer = mesh->indexBuffer->getBuffer();

            // Bind Geometry
            vkCmdBindVertexBuffers(cmd, 0, 1, &vBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, iBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw
            vkCmdDrawIndexed(
                cmd,
                mesh->indexCount,     // indices in this mesh
                batch.instanceCount,  // how many of this mesh exist in this batch
                0,                    // firstIndex
                0,                    // vertexOffset
                batch.firstInstance   // Offset gl_InstanceIndex to point to the right slice of SSBO
            );
        }

        vkCmdEndRendering(cmd);
    }
}
