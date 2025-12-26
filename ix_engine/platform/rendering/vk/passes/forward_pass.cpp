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
#include "global_common/ix_gpu_types.h"


namespace ix
{
    ForwardPass::ForwardPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ForwardPass::setup(RenderGraphBuilder& builder) {
        builder.write("BackBuffer");
        builder.write("DepthBuffer");
        m_cachedPipeline = builder.getPipelineManager()->getGraphicsPipeline("ForwardPass");
    }

    void ForwardPass::execute(const FrameContext& ctx, RenderGraphRegistry& registry)
    {
        if (!m_cachedPipeline) return;

        auto& scene = SceneManager::getActiveScene();
        auto& colorState = registry.getResourceState("BackBuffer");
        auto& depthState = registry.getResourceState("DepthBuffer");

        VulkanImage* targetImage = colorState.physicalImage;
        VulkanImage* depthImage = depthState.physicalImage;

        if (!targetImage || !depthImage) return;

        VkExtent2D extent = targetImage->getExtent();

        // Rendering Attachments
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

        vkCmdBeginRendering(ctx.commandBuffer, &renderingInfo);

        VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
        vkCmdSetViewport(ctx.commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(ctx.commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(ctx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedPipeline->getHandle());

        VkDescriptorSet sets[] = { ctx.globalDescriptorSet, ctx.bindlessDescriptorSet };
        vkCmdBindDescriptorSets(ctx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_cachedPipeline->getLayout(), 0, 2, sets, 0, nullptr);

        // Render loop
        scene.getRegistry().view<MeshComponent, TransformComponent>().each([&](auto entity, auto& meshComp, auto& transformComp) {
            VulkanMesh* mesh = AssetManager::get().getMesh(meshComp.meshHandle);
            if (!mesh) return;

            struct {
                glm::mat4 model;
                int textureIndex;
            } pushData;

            pushData.model = transformComp.getTransform();
            pushData.textureIndex = 3;

            vkCmdPushConstants(
                ctx.commandBuffer,
                m_cachedPipeline->getLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(pushData),
                &pushData
            );

            VkDeviceSize offsets[] = { 0 };

            VkBuffer vBuffer = mesh->vertexBuffer->getBuffer();

            VkBuffer iBuffer = mesh->indexBuffer->getBuffer();

            vkCmdBindVertexBuffers(ctx.commandBuffer, 0, 1, &vBuffer, offsets);

            vkCmdBindIndexBuffer(ctx.commandBuffer, iBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(ctx.commandBuffer, mesh->indexCount, 1, 0, 0, 0);

            });

        vkCmdEndRendering(ctx.commandBuffer);
    }
}
