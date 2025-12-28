#include "common/engine_pch.h"
#include "skybox_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_image.h"

#include "engine.h"
#include "core/scene_manager.h"
#include "core/components.h"
#include "global_common/ix_global_pods.h"




namespace ix
{

	SkyboxPass::SkyboxPass(const std::string& name) : RenderGraphPass_I(name) {}

	void SkyboxPass::setup(RenderGraphBuilder& builder)
	{
        m_cachedPipeline = builder.getPipelineManager()->getGraphicsPipeline("Skybox");

        if (!m_cachedPipeline) {
            spdlog::error("SkyboxPass: Could not find pipeline 'Skybox' in manager!");
        }
	}

    void SkyboxPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {

        VkCommandBuffer cmd = state.frame.commandBuffer;
        auto& scene = SceneManager::getActiveScene();
        TextureHandle skyHandle = scene.getSkybox();
        if (skyHandle == 0) {
            spdlog::warn("SkyboxPass: No skybox handle set in scene!");
            return;
        }
        auto& colorState = registry.getResourceState("BackBuffer");
        auto& depthState = registry.getResourceState("DepthBuffer");

        VulkanImage* targetImage = colorState.physicalImage;
        VulkanImage* depthImage = depthState.physicalImage;

        if (!targetImage || !depthImage) return;

        VkExtent2D extent = targetImage->getExtent();

        VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.imageView = targetImage->getView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachment.imageView = depthImage->getView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, extent };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        if (m_cachedPipeline)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cachedPipeline->getHandle());

            VkDescriptorSet sets[] = { state.frame.globalDescriptorSet, state.frame.bindlessDescriptorSet };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_cachedPipeline->getLayout(), 0, 2, sets, 0, nullptr);

            int skyboxIdx = AssetManager::get().getTextureBindlessIndex(skyHandle);

            uint32_t stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            vkCmdPushConstants(cmd, m_cachedPipeline->getLayout(),
                stages,
                64, sizeof(int), &skyboxIdx);

            vkCmdDraw(cmd, 36, 1, 0, 0);
        }

        vkCmdEndRendering(cmd);
    }
}