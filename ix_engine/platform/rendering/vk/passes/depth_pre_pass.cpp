#include "common/engine_pch.h"
#include "depth_pre_pass.h"
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

    DepthPrePass::DepthPrePass(const std::string& name) : RenderGraphPass_I(name) {}

    void DepthPrePass::setup(RenderGraphBuilder& builder)
    {
        builder.write("DepthBuffer");
        m_cachedPipeline = builder.getPipelineManager()->getGraphicsPipeline("DepthPrePass");
    }

    void DepthPrePass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        if (!m_cachedPipeline || state.frame.instanceCount == 0) return;

        VkCommandBuffer cmd = state.frame.commandBuffer;
        auto& depthState = registry.getResourceState("DepthBuffer");
        VulkanImage* depthImage = depthState.physicalImage;
        VulkanBuffer* culledBuffer = registry.getBuffer("CulledInstances");

        VkExtent2D extent = depthImage->getExtent();

        // Setup Depth-Only Rendering (No color attachments!)
        VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachment.imageView = depthImage->getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear to 1.0
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, extent };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 0; // Explicitly 0
        renderingInfo.pColorAttachments = nullptr;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Standard Dynamic States
        VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedPipeline->getHandle());

        // Reuse the same descriptor sets (Global UBO and Instance Data)
        VkDescriptorSet sets[] = {
           state.frame.globalDescriptorSet,
           state.frame.bindlessDescriptorSet,
           state.frame.instanceDescriptorSet
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_cachedPipeline->getLayout(), 0, 3, sets, 0, nullptr);

        // Bind Global Geometry
        VkBuffer vboHandle = AssetManager::get().getGlobalVBO()->getBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vboHandle, &offset);
        vkCmdBindIndexBuffer(cmd, AssetManager::get().getGlobalIBO()->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Draw everything
        vkCmdDrawIndexedIndirect(cmd, culledBuffer->getBuffer(), 0,
            state.frame.renderBatches->size(), sizeof(GPUIndirectCommand));

        vkCmdEndRendering(cmd);
    }
}