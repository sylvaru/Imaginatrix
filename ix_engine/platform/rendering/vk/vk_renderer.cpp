#include "common/engine_pch.h"
#include "vk_renderer.h"

namespace ix
{
	VulkanRenderer::VulkanRenderer(Window_I& window)
		: m_window(window)
	{
	}
	VulkanRenderer::~VulkanRenderer()
	{
		shutdown();
	}
	void VulkanRenderer::init()
	{
		m_instance = std::make_unique<VulkanInstance>("Imaginatrix Engine");
		m_context = std::make_unique<VulkanContext>(*m_instance, m_window);
	}
	void VulkanRenderer::shutdown()
	{
		if (m_context) {
			vkDeviceWaitIdle(m_context->device());
		}
	}
	void VulkanRenderer::onResize(uint32_t width, uint32_t height)
	{
		vkDeviceWaitIdle(m_context->device());
		recreateSwapchain();
	}
	void VulkanRenderer::beginFrame()
	{
		//VkCommandBuffer cmd = getCurrentCommandBuffer();

		if (m_context->useDynamicRendering()) {
			VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };

			VkRenderingAttachmentInfo colorAttachment{};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			//colorAttachment.imageView = m_swapchain->getCurrentImageView();
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue = clearColor;

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			//renderingInfo.renderArea = { {0, 0}, m_swapchain->getExtent() };
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;

			//vkCmdBeginRendering(cmd, &renderingInfo);
		}
		else {
			// Legacy Path: Use the VkRenderPass you created in init()
			// m_swapchain->beginLegacyRenderPass(cmd); 
		}
	}
	void VulkanRenderer::endFrame()
	{
		//VkCommandBuffer cmd = getCurrentCommandBuffer();

		//if (m_context->useDynamicRendering()) {
		//	vkCmdEndRendering(cmd);
		//}
		//else {
		//	vkCmdEndRenderPass(cmd);
		//}
	}
	void VulkanRenderer::submitScene(Scene& scene, float alpha)
	{
		std::vector<DrawCommand> drawList;
		drawList.reserve(1000);

		// Collect
		//auto view = scene.getRegistry().view<TransformComponent, MeshComponent, MaterialComponent>();
		//for (auto entity : view) {
		//	auto& [transform, mesh, material] = view.get<TransformComponent, MeshComponent, MaterialComponent>(entity);

		// Culling
		//	// if (!camera.sees(mesh.bounds)) continue;

		//	drawList.push_back({
		//		material.pipelineID,
		//		material.id,
		//		mesh.vbo,
		//		mesh.ibo,
		//		mesh.count,
		//		transform.getMatrix(alpha)
		//		});
		//}


		// Sort (This is where performance comes from)
		std::sort(drawList.begin(), drawList.end(), [](const DrawCommand& a, const DrawCommand& b) {
			if (a.pipelineID != b.pipelineID) return a.pipelineID < b.pipelineID;
			return a.materialID < b.materialID;
			});

		// Draw
		uint32_t lastPipeline = 0xFFFFFFFF;
		for (const auto& dc : drawList) {
			if (dc.pipelineID != lastPipeline) {
				//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[dc.pipelineID]);
				lastPipeline = dc.pipelineID;
			}
			// Bind mesh and draw
		}

	}
	void VulkanRenderer::recreateSwapchain()
	{
	}
	void VulkanRenderer::createCommandBuffers()
	{
	}
	void VulkanRenderer::createSyncObjects()
	{
	}
}

