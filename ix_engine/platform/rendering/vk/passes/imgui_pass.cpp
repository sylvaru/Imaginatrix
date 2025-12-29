#include "common/engine_pch.h"
#include "imgui_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_image.h"

#include "engine.h"
#include "global_common/ix_global_pods.h"
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace ix
{
    ImGuiPass::ImGuiPass(const std::string& name) : RenderGraphPass_I(name) {}

    void ImGuiPass::setup(RenderGraphBuilder& builder)
    {
        builder.write("BackBuffer");
    }

    void ImGuiPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        ImDrawData* drawData = ImGui::GetDrawData();

        if (!drawData || drawData->TotalVtxCount == 0) return;

        VkCommandBuffer cmd = state.frame.commandBuffer;

        // Get Resource State and Physical Image
        auto& colorState = registry.getResourceState("BackBuffer");
        VulkanImage* targetImage = colorState.physicalImage;

        if (!targetImage) return;

        // Extract Extent
        VkExtent2D extent = targetImage->getExtent();

        // Setup Rendering Attachment
        VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.imageView = targetImage->getView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, extent };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = nullptr;

        // Begin Dynamic Rendering
        vkCmdBeginRendering(cmd, &renderingInfo);

        // Record ImGui primitives
        ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

        // End Dynamic Rendering
        vkCmdEndRendering(cmd);
    }
}
