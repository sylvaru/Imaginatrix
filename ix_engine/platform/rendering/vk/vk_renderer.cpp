// vk_renderer.cpp
#include "common/engine_pch.h"
#include "vk_renderer.h"
#include "vk_instance.h"
#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_image.h"	
#include "vk_pipeline.h"
#include "vk_pipeline_manager.h"
#include "vk_descriptor_manager.h"
#include "platform/rendering/vk/render_graph/vk_render_graph.h"
#include "platform/rendering/vk/passes/forward_pass.h"
#include "window_i.h"

namespace ix
{
	VulkanRenderer::VulkanRenderer(Window_I& window) 
		: m_window(window)
		, m_instance(std::make_unique<VulkanInstance>("Imaginatrix Engine"))
		, m_context(std::make_unique<VulkanContext>(*m_instance, m_window))
		, m_pipelineManager(std::make_unique<VulkanPipelineManager>(*m_context))
		, m_descriptorManager(std::make_unique<VulkanDescriptorManager>(*m_context))
		, m_renderGraph(std::make_unique<RenderGraph>())
	{

	}
	VulkanRenderer::~VulkanRenderer() { shutdown(); }

	void VulkanRenderer::init()
	{
		recreateSwapchain();
		createSyncObjects();
		createCommandBuffers();
		createGlobalLayout();

		m_globalUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_globalUboBuffers[i] = std::make_unique<VulkanBuffer>(
				*m_context,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_CPU_TO_GPU);
			m_globalUboBuffers[i]->map();
		}

		// Setup passes for graph
		auto forwardPass = std::make_unique<ForwardPass>("MainForward");
		m_renderGraph->addPass(std::move(forwardPass));
		m_renderGraph->compile();

		spdlog::info("Vulkan Renderer: Initialized with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
	}

	void VulkanRenderer::recreateSwapchain()
	{
		spdlog::info("Recreating Swapchain...");
		waitIdle();

		int w, h;
		m_window.getFramebufferSize(w, h);
		while (w == 0 || h == 0)
		{
			m_window.getFramebufferSize(w, h);
			m_window.waitEvents();
		} 
		
		auto newSwapchain = std::make_unique<VulkanSwapchain>(
			*m_context,
			VkExtent2D{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) },
			m_swapchain.get()
		);
	
		// Set the new one as current
		m_swapchain = std::move(newSwapchain);

		m_context->setSwapchainFormat(m_swapchain->getFormat());
		m_context->setDepthFormat(VK_FORMAT_D32_SFLOAT);

		m_depthImage = std::make_unique<VulkanImage>(
			*m_context,
			m_swapchain->getExtent(),
			VK_FORMAT_D32_SFLOAT,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		m_pipelineManager->reloadPipelines();

		if (m_renderGraph) {
			m_renderGraph->clearExternalResources();

			uint32_t startIdx = 0;
			m_renderGraph->importImage("BackBuffer", m_swapchain->getImageWrapper(startIdx));
			m_renderGraph->importImage("DepthBuffer", m_depthImage.get());
			m_renderGraph->compile();
		}
		spdlog::info("Recreating Swapchain: Finished successfully");
		
	}

	bool VulkanRenderer::beginFrame(FrameContext& ctx)
	{
		FrameData& frame = getCurrentFrame();

		vkWaitForFences(m_context->device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = m_swapchain->acquireNextImage(frame.imageAvailableSemapohore, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return false;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		m_currentImageIndex = imageIndex;

		ctx.commandBuffer = frame.commandBuffer;
		ctx.frameIndex = m_currentFrameIndex;
		ctx.pipelineManager = m_pipelineManager.get();


		// Reset Fence & Command Buffer for recording
		vkResetFences(m_context->device(), 1, &frame.inFlightFence);
		vkResetCommandBuffer(frame.commandBuffer, 0);


		updateGlobalUbo(ctx);

		// Descriptor Management
		VkDescriptorSet globalSet;
		if (!m_descriptorManager->allocate(&globalSet, m_globalDescriptorLayout)) {
			spdlog::error("VulkanRenderer: Failed to allocate global descriptor set!");
		}

		// Update the set using DescriptorWriter
		auto bufferInfo = m_globalUboBuffers[m_currentFrameIndex]->descriptorInfo();
		DescriptorWriter writer;
		writer.writeBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.updateSet(*m_context, globalSet);

		// Command Recording Start
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		// Initial Bindings
		vkCmdBindDescriptorSets(
			frame.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_defaultLayout,
			0, 1, &globalSet,
			0, nullptr
		);

		return true;
	}

	void VulkanRenderer::render(const FrameContext& ctx)
	{
		VulkanImage* currentImg = m_swapchain->getImageWrapper(m_currentImageIndex);
		if (currentImg) {
			m_renderGraph->importImage("BackBuffer", currentImg);
			m_renderGraph->importImage("DepthBuffer", m_depthImage.get());
		}
		m_renderGraph->execute(ctx);
	}

	void VulkanRenderer::endFrame(const FrameContext& ctx)
	{
		FrameData& frame = getCurrentFrame();


		// Transition to Present
		VulkanImage* currentImg = m_swapchain->getImageWrapper(m_currentImageIndex);
		currentImg->transition(frame.commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		vkEndCommandBuffer(frame.commandBuffer);

		// Submit
		VkSemaphore renderFinishedSemaphore = m_swapchain->getRenderSemaphore(m_currentImageIndex);
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &frame.imageAvailableSemapohore;
		submit.pWaitDstStageMask = waitStages;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &frame.commandBuffer;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &renderFinishedSemaphore;

		if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submit, frame.inFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		// Present
		VkSwapchainKHR swapchains[] = { m_swapchain->get() };
		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &m_currentImageIndex;

		VkResult result = vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapchain();
		}

		// Reset the descriptor pools for the next time we use this frame index
		m_descriptorManager->resetPools();

		m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}


	void VulkanRenderer::onResize(uint32_t width, uint32_t height)
	{
		vkDeviceWaitIdle(m_context->device());
		recreateSwapchain();
	}

	RenderExtent VulkanRenderer::getSwapchainExtent() const 
	{
		auto extent = m_swapchain->getExtent();
		return { extent.width, extent.height };
	}

	void VulkanRenderer::updateGlobalUbo(const FrameContext& ctx) 
	{
		// Map the context data to our GPU-aligned struct
		m_globalUboData.projection = ctx.projectionMatrix;
		m_globalUboData.view = ctx.viewMatrix;
		m_globalUboData.cameraPos = glm::vec4(ctx.cameraPosition, 1.0f);
		m_globalUboData.time = ctx.totalTime;
		m_globalUboData.deltaTime = ctx.deltaTime;

		// Upload to the buffer specific to this frame in flight
		m_globalUboBuffers[m_currentFrameIndex]->writeToBuffer(&m_globalUboData);
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



	void VulkanRenderer::loadPipelines(const nlohmann::json& json)
	{

		std::string shaderRoot = std::string(PROJECT_ROOT_DIR) + "/sandbox_game/res/shaders/spirV/";

		for (const auto& entry : json["pipelines"]) {
			PipelineState state;

			// Map JSON state to Vulkan Enums
			state.topology = (entry["state"]["topology"] == "triangle_list") ?
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

			state.polygonMode = (entry["state"]["polygonMode"] == "line") ?
				VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

			state.cullMode = (entry["state"]["cullMode"] == "back") ?
				VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;

			state.depthTest = entry["state"]["depthTest"].get<bool>() ? VK_TRUE : VK_FALSE;
			state.depthWrite = entry["state"]["depthWrite"].get<bool>() ? VK_TRUE : VK_FALSE;

			state.colorAttachmentFormats = { m_swapchain->getFormat() };
			state.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

			std::string vertPath = shaderRoot + entry["vert"].get<std::string>();
			std::string fragPath = shaderRoot + entry["frag"].get<std::string>();

			m_pipelineManager->createGraphicsPipeline(
				entry["name"],
				vertPath,
				fragPath,
				state,
				m_defaultLayout
			);

			spdlog::info("VulkanRenderer: Created pipeline '{}' using shaders from {}",
				entry["name"].get<std::string>(), shaderRoot);
		}
	}

	void VulkanRenderer::createGlobalLayout() 
	{
		// Define the Binding for the Global UBO
		VkDescriptorSetLayoutBinding globalUboBinding{};
		globalUboBinding.binding = 0;
		globalUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		globalUboBinding.descriptorCount = 1;
		globalUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		globalUboBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descLayoutCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		descLayoutCI.bindingCount = 1;
		descLayoutCI.pBindings = &globalUboBinding;

		if (vkCreateDescriptorSetLayout(m_context->device(), &descLayoutCI, nullptr, &m_globalDescriptorLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout!");
		}

		// Push Constants
		VkPushConstantRange pushConstant{};
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(glm::mat4);

		VkPipelineLayoutCreateInfo pipelineLayoutCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &m_globalDescriptorLayout;
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstant;

		if (vkCreatePipelineLayout(m_context->device(), &pipelineLayoutCI, nullptr, &m_defaultLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout!");
		}
	}

	void VulkanRenderer::waitIdle()
	{
		vkDeviceWaitIdle(m_context->device());
	}


	void VulkanRenderer::shutdown()
	{
		if (!m_context) return;

		if (m_pipelineManager) {
			m_pipelineManager->clearCache();
			m_pipelineManager.reset();
		}

		if (m_renderGraph) {
			m_renderGraph->clearExternalResources();
			m_renderGraph.reset();
		}
		m_depthImage.reset();
		m_descriptorManager.reset();

		m_globalUboBuffers.clear();

		if (m_defaultLayout != VK_NULL_HANDLE) 
		{
			vkDestroyPipelineLayout(m_context->device(), m_defaultLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_context->device(), m_globalDescriptorLayout, nullptr);
			m_defaultLayout = VK_NULL_HANDLE;
		}

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
		{
			vkDestroySemaphore(m_context->device(), m_frames[i].imageAvailableSemapohore, nullptr);
			vkDestroySemaphore(m_context->device(), m_frames[i].renderFinishedSemaphore, nullptr);
			vkDestroyFence(m_context->device(), m_frames[i].inFlightFence, nullptr);
			if (m_frames[i].commandPool != VK_NULL_HANDLE) {
				vkDestroyCommandPool(m_context->device(), m_frames[i].commandPool, nullptr);
			}
		}

		m_swapchain.reset();
		m_context.reset();
		m_instance.reset();
	}

}

