// vk_renderer.cpp
#include "common/engine_pch.h"
#include "vk_renderer.h"
#include "vk_instance.h"
#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_image.h"	

#include "window_i.h"


namespace ix
{
	VulkanRenderer::VulkanRenderer(Window_I& window) : m_window(window) {}
	VulkanRenderer::~VulkanRenderer() { shutdown(); }

	void VulkanRenderer::init()
	{
		m_instance = std::make_unique<VulkanInstance>("Imaginatrix Engine");
		m_context = std::make_unique<VulkanContext>(*m_instance, m_window);

		recreateSwapchain();
		createSyncObjects();
		createCommandBuffers();

		spdlog::info("Vulkan Renderer: Initialized with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
	}

	void VulkanRenderer::shutdown() 
	{
		if (!m_context) return;
		vkDeviceWaitIdle(m_context->device());

		m_swapchain.reset();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(m_context->device(), m_frames[i].imageAvailableSemapohore, nullptr);
			vkDestroySemaphore(m_context->device(), m_frames[i].renderFinishedSemaphore, nullptr);
			vkDestroyFence(m_context->device(), m_frames[i].inFlightFence, nullptr);
			vkDestroyCommandPool(m_context->device(), m_frames[i].commandPool, nullptr);
		}

		m_context.reset();
		m_instance.reset();
	}
	void VulkanRenderer::onResize(uint32_t width, uint32_t height)
	{
		vkDeviceWaitIdle(m_context->device());
		recreateSwapchain();
	}
	void VulkanRenderer::beginFrame()
	{
		FrameData& frame = getCurrentFrame();

		// Wait for GPU to finish the previous frame using this index
		vkWaitForFences(m_context->device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

		// Acquire image from swapchain
		VkResult result = m_swapchain->acquireNextImage(frame.imageAvailableSemapohore, &m_currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}

		vkResetFences(m_context->device(), 1, &frame.inFlightFence);

		// Start Recording
		vkResetCommandBuffer(frame.commandBuffer, 0);
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

		// Transition Swapchain Image
		// This updates the internal layout state of the VulkanImage object
		VulkanImage* currentImg = m_swapchain->getImageWrapper(m_currentImageIndex);
		currentImg->transition(frame.commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		if (m_context->useDynamicRendering()) {
			VkClearValue clearColor = { {{ 0.5f, 0.5f, 0.5f, 1.0f }} };

			VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			colorAttachment.imageView = currentImg->getView(); 
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue = clearColor;

			VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
			renderingInfo.renderArea = { {0, 0}, m_swapchain->getExtent() };
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;

			vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);
		}
	}

	void VulkanRenderer::endFrame()
	{
		FrameData& frame = getCurrentFrame();

		if (m_context->useDynamicRendering()) {
			vkCmdEndRendering(frame.commandBuffer);
		}

		// Transition Swapchain Image to Present Layout
		VulkanImage* currentImg = m_swapchain->getImageWrapper(m_currentImageIndex);
		currentImg->transition(frame.commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		vkEndCommandBuffer(frame.commandBuffer);

		// Submit to Graphics Queue
		VkSemaphore renderFinishedSemaphore = m_swapchain->getRenderSemaphore(m_currentImageIndex);

		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &frame.imageAvailableSemapohore;
		submit.pWaitDstStageMask = waitStages;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &frame.commandBuffer;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &renderFinishedSemaphore; // Per-image semaphore

		vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submit, frame.inFlightFence);

		// Present
		VkSwapchainKHR swapchains[] = { m_swapchain->get() };
		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for the image-specific semaphore
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &m_currentImageIndex;

		VkResult result = vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapchain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swapchain image!");
		}

		m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	void VulkanRenderer::recreateSwapchain()
	{
		int w, h;
		m_window.getFramebufferSize(w, h);
		do
		{
			m_window.getFramebufferSize(w, h);
			m_window.waitEvents();
		} while (w == 0 || h == 0);

		vkDeviceWaitIdle(m_context->device());

		auto newSwapchain = std::make_unique<VulkanSwapchain>(
			*m_context,
			VkExtent2D{ 
			static_cast<uint32_t>(w),
			static_cast<uint32_t>(h)
			},
			m_swapchain.get()
		);
		m_swapchain = std::move(newSwapchain);
	}

	void VulkanRenderer::createCommandBuffers() 
	{
		uint32_t graphicsFamily = m_context->getGraphicsFamily();
		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.queueFamilyIndex = graphicsFamily;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkCreateCommandPool(m_context->device(), &poolInfo, nullptr, &m_frames[i].commandPool);

			VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			allocInfo.commandPool = m_frames[i].commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			vkAllocateCommandBuffers(m_context->device(), &allocInfo, &m_frames[i].commandBuffer);
		}
	}

	void VulkanRenderer::createSyncObjects() 
	{
		VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so we don't wait forever on frame 0

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkCreateSemaphore(m_context->device(), &semInfo, nullptr, &m_frames[i].imageAvailableSemapohore);
			vkCreateSemaphore(m_context->device(), &semInfo, nullptr, &m_frames[i].renderFinishedSemaphore);
			vkCreateFence(m_context->device(), &fenceInfo, nullptr, &m_frames[i].inFlightFence);
		}
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

}

