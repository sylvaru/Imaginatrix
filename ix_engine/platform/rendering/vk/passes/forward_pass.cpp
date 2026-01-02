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
        builder.read("DepthBuffer");
        m_cachedPipeline = builder.getPipelineManager()->getGraphicsPipeline("ForwardPass");
    }

    void ForwardPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        if (!m_cachedPipeline || state.frame.instanceCount == 0) return;

        VkCommandBuffer cmd = state.frame.commandBuffer;
        auto& colorState = registry.getResourceState("BackBuffer");
        auto& depthState = registry.getResourceState("DepthBuffer");

        VulkanBuffer* culledBuffer = registry.getBuffer("CulledInstances");
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
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

        // Bind global VBO/IBO
        VulkanBuffer* vboWrapper = AssetManager::get().getGlobalVBO();
        VulkanBuffer* iboWrapper = AssetManager::get().getGlobalIBO();
        VkDeviceSize offset = 0;
        VkBuffer vboHandle = vboWrapper->getBuffer();

        vkCmdBindVertexBuffers(cmd, 0, 1, &vboHandle, &offset);
        vkCmdBindIndexBuffer(cmd, iboWrapper->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Log
        static bool debug_log_once = true;
        if (debug_log_once) {
            spdlog::info("--- Forward Pass ---");
            uint32_t totalInstances = 0;
            for (size_t i = 0; i < state.frame.renderBatches->size(); ++i) {
                const auto& batch = (*state.frame.renderBatches)[i];
                auto* mesh = AssetManager::get().getMesh(batch.meshHandle);

                spdlog::info("Batch [{}] (Mesh: {}):", i, batch.meshHandle);
                spdlog::info("  -> Instances: {} (Starting at Global Index: {})",
                    batch.instanceCount, batch.firstInstance);

                if (mesh) {
                    spdlog::info("  -> Geometry: BaseVertex: {}, FirstIndex: {}, Count: {}",
                        mesh->baseVertex, mesh->firstIndex, mesh->indexCount);
                }
                totalInstances += batch.instanceCount;
            }

            spdlog::info("Total Indirect Commands: {} | Total Potential Instances: {}",
                state.frame.renderBatches->size(), totalInstances);

            debug_log_once = false;
        }

        // Draw
        vkCmdDrawIndexedIndirect(
            cmd,
            culledBuffer->getBuffer(),
            0,
            state.frame.renderBatches->size(),
            sizeof(GPUIndirectCommand)
        );


        vkCmdEndRendering(cmd);
    }
}
