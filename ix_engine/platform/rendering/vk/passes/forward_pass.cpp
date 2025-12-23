#include "common/engine_pch.h"
#include "forward_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_pipeline.h"
#include "platform/rendering/vk/vk_buffer.h"
#include "platform/rendering/vk/vk_renderer.h"
#include "platform/rendering/vk/vk_swapchain.h"
#include "platform/rendering/vk/vk_image.h"

#include "engine.h"
#include "core/asset_manager.h"
#include "core/scene_manager.h"
#include "core/components.h"
#include "global_common/ix_gpu_types.h"


namespace ix
{
    ForwardPass::ForwardPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ForwardPass::setup(RenderGraphBuilder& builder) {
        builder.write("BackBuffer");
        builder.write("DepthBuffer");
    }

    void ForwardPass::execute(const FrameContext& ctx, RenderGraphRegistry& registry) 
    {
        auto& scene = SceneManager::getActiveScene();

        auto& colorState = registry.getResourceState("BackBuffer");
        auto& depthState = registry.getResourceState("DepthBuffer");

        VulkanImage* targetImage = colorState.physicalImage;
        VulkanImage* depthImage = depthState.physicalImage;

        if (!targetImage || !depthImage) return;

        VkExtent2D extent = targetImage->getExtent();
       
        // Color Attachment Setup
        VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.imageView = targetImage->getView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {{ 0.1f, 0.1f, 0.1f, 1.0f }} };

        // Depth Attachment Setup
        VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachment.imageView = depthImage->getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 }; // Clear to far plane

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, extent };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        // Start Dynamic Rendering
        vkCmdBeginRendering(ctx.commandBuffer, &renderingInfo);

        // Set Dynamic State
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(ctx.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(ctx.commandBuffer, 0, 1, &scissor);


        // Pipeline Setup
        PipelineState pipelineState{};
        pipelineState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineState.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineState.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        pipelineState.depthTest = VK_TRUE;
        pipelineState.depthWrite = VK_TRUE;
        pipelineState.depthCompareOp = VK_COMPARE_OP_LESS;

        auto* pipeline = ctx.pipelineManager->getGraphicsPipeline(pipelineState);
        if (!pipeline) 
        {
            spdlog::critical("BLACK SCREEN DETECTED: Pipeline not found in cache after resize!");
            return;
        }

        if (pipeline) 
        {
            vkCmdBindPipeline(ctx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getHandle());

            // Render loop
            scene.getRegistry().view<MeshComponent, TransformComponent>().each([&](auto entity, auto& meshComp, auto& transformComp) {
                VulkanMesh* mesh = AssetManager::get().getMesh(meshComp.meshHandle);
                if (!mesh) return;

                VkPipelineLayout layout = pipeline->getLayout();
                VkBuffer vertBuf = mesh->vertexBuffer->getBuffer();
                VkBuffer ivkBuf = mesh->indexBuffer->getBuffer();

                // Push Constants
                glm::mat4 model = transformComp.getTransform();
                vkCmdPushConstants(ctx.commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

                // Bind and Draw
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(ctx.commandBuffer, 0, 1, &vertBuf, offsets);
                vkCmdBindIndexBuffer(ctx.commandBuffer, ivkBuf, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(ctx.commandBuffer, mesh->indexCount, 1, 0, 0, 0);
                });
        }

        vkCmdEndRendering(ctx.commandBuffer);
    }
}