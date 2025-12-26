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
#include "platform/rendering/vk/passes/skybox_pass.h"
#include "platform/rendering/vk/passes/compute_pass.h"
#include "window_i.h"
#include "core/asset_manager.h"
#include "core/scene_manager.h"

namespace ix
{
	VulkanRenderer::VulkanRenderer(Window_I& window) 
		: m_window(window)
		, m_instance(std::make_unique<VulkanInstance>("Imaginatrix Engine"))
		, m_context(std::make_unique<VulkanContext>(*m_instance, m_window))
		, m_pipelineManager(std::make_unique<VulkanPipelineManager>(*m_context))
		, m_renderGraph(std::make_unique<RenderGraph>())
	{
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
		{
			m_descriptorManagers.push_back(std::make_unique<VulkanDescriptorManager>(*m_context));
		}

	}
	VulkanRenderer::~VulkanRenderer() { shutdown(); }

	void VulkanRenderer::init()
	{
		recreateSwapchain();
		createSyncObjects();
		createCommandBuffers();
		createGlobalLayout();

		m_pipelineManager->setDefaultLayout(m_defaultLayout);

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

		m_bindlessPool = m_descriptorManagers[0]->createBindlessPool(1, 1000);
		m_descriptorManagers[0]->allocateBindless(m_bindlessPool, &m_bindlessDescriptorSet, m_bindlessLayout, 1000);


		spdlog::info("Vulkan Renderer: Initialized with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
	}
	void VulkanRenderer::compileRenderGraph()
	{
		auto forwardPass = std::make_unique<ForwardPass>("MainForward");
		auto computePass = std::make_unique<ComputePass>("ComputePass");
		auto skyboxPass = std::make_unique<SkyboxPass>("SkyboxPass");

		m_renderGraphCompileConfig.context = m_context.get();
		m_renderGraphCompileConfig.pipelineManager = m_pipelineManager.get();

		m_renderGraph->addPass(std::move(forwardPass));
		m_renderGraph->addPass(std::move(computePass));
		m_renderGraph->addPass(std::move(skyboxPass));
		m_renderGraph->compile(m_renderGraphCompileConfig);
	}
	
	bool VulkanRenderer::beginFrame(FrameContext& ctx)
	{
		FrameData& frame = getCurrentFrame();

		vkWaitForFences(m_context->device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

		// Reset this frames pools
		auto& dynamicDescriptorManager = m_descriptorManagers[m_currentFrameIndex];
		dynamicDescriptorManager->resetPools();

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

		// Reset Fence & Command Buffer for recording
		vkResetFences(m_context->device(), 1, &frame.inFlightFence);
		vkResetCommandBuffer(frame.commandBuffer, 0);

		// Descriptor Management
		VkDescriptorSet globalSet;
		if (!dynamicDescriptorManager->allocate(&globalSet, m_globalDescriptorLayout)) {
			spdlog::error("VulkanRenderer: Failed to allocate global descriptor set!");
		}

		// Update the set using DescriptorWriter
		auto bufferInfo = m_globalUboBuffers[m_currentFrameIndex]->descriptorInfo();
		DescriptorWriter writer;
		writer.writeBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.updateSet(*m_context, globalSet);

		// Populate FrameContext
		ctx.frameIndex = m_currentFrameIndex;
		ctx.commandBuffer = frame.commandBuffer;
		ctx.globalDescriptorSet = globalSet;
		ctx.bindlessDescriptorSet = m_bindlessDescriptorSet;
		ctx.computeStorageLayout = m_computeStorageLayout;
		ctx.context = m_context.get();
		ctx.pipelineManager = m_pipelineManager.get();
		ctx.descriptorManager = m_descriptorManagers[m_currentFrameIndex].get();

		updateGlobalUbo(ctx);

		// Command Recording Start
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		return true;
	}

	void VulkanRenderer::render(const FrameContext& ctx)
	{
		updateBindlessTextures(AssetManager::get().takePendingUpdates());

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

		m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void VulkanRenderer::loadPipelines(const nlohmann::json& json)
	{
		std::string shaderRoot = std::string(PROJECT_ROOT_DIR) + "/sandbox_game/res/shaders/spirV/";


		for (const auto& entry : json["pipelines"]) {


			std::string type = entry.value("type", "graphics");
			std::string name = entry["name"];

			if (type == "compute") {
				// Compute Pipeline Path
				std::string compPath = shaderRoot + entry["compute"].get<std::string>();

				m_pipelineManager->createComputePipeline(name, compPath, m_defaultLayout);

				spdlog::info("VulkanRenderer: Created compute pipeline '{}'", name);
			}
			else {
				// Graphics Pipeline Path
				PipelineState state;
				auto& sJson = entry["state"];

				state.isProcedural = sJson.value("procedural", false);

				state.topology = (sJson["topology"] == "triangle_list") ?
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

				state.polygonMode = (sJson["polygonMode"] == "line") ?
					VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

				state.cullMode = m_pipelineManager->parseCullMode(sJson.value("cullMode", "back"));
				state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

				state.depthTest = sJson.value("depthTest", true) ? VK_TRUE : VK_FALSE;
				state.depthWrite = sJson.value("depthWrite", true) ? VK_TRUE : VK_FALSE;

				state.depthCompareOp = (sJson.value("compareOp", "less") == "less_or_equal") ?
					VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;

				state.colorAttachmentFormats = { m_swapchain->getFormat() };
				state.depthAttachmentFormat = m_context->getDepthFormat();

				std::string vertPath = shaderRoot + entry["vert"].get<std::string>();
				std::string fragPath = shaderRoot + entry["frag"].get<std::string>();

				m_pipelineManager->createGraphicsPipeline(name, vertPath, fragPath, state, m_defaultLayout);

				spdlog::info("VulkanRenderer: Created graphics pipeline '{}'", name);
			}
		}
	}

	void VulkanRenderer::createGlobalLayout()
	{
		// Set 0: Global UBO
		VkDescriptorSetLayoutBinding globalUboBinding{};
		globalUboBinding.binding = 0;
		globalUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		globalUboBinding.descriptorCount = 1;
		globalUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo uboLayoutCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		uboLayoutCI.bindingCount = 1;
		uboLayoutCI.pBindings = &globalUboBinding;

		if (vkCreateDescriptorSetLayout(m_context->device(), &uboLayoutCI, nullptr, &m_globalDescriptorLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create UBO descriptor set layout!");
		}

		// Set 1: Bindless Textures
		VkDescriptorSetLayoutBinding bindlessBinding{};
		bindlessBinding.binding = 0;
		bindlessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindlessBinding.descriptorCount = 1000; // Max textures
		bindlessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		// Flags for bindless
		VkDescriptorBindingFlags flags =
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		bindingFlags.bindingCount = 1;
		bindingFlags.pBindingFlags = &flags;

		VkDescriptorSetLayoutCreateInfo bindlessLayoutCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		bindlessLayoutCI.pNext = &bindingFlags;
		bindlessLayoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		bindlessLayoutCI.bindingCount = 1;
		bindlessLayoutCI.pBindings = &bindlessBinding;

		if (vkCreateDescriptorSetLayout(m_context->device(), &bindlessLayoutCI, nullptr, &m_bindlessLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create bindless descriptor set layout!");
		}

		// Set 2: Storage image for compute
		VkDescriptorSetLayoutBinding storageBinding{};
		storageBinding.binding = 0;
		storageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		storageBinding.descriptorCount = 1;
		storageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &storageBinding;

		vkCreateDescriptorSetLayout(m_context->device(), &layoutInfo, nullptr, &m_computeStorageLayout);

		// Pipeline Layout
		std::vector<VkDescriptorSetLayout> layouts = { m_globalDescriptorLayout, m_bindlessLayout, m_computeStorageLayout };

		VkPushConstantRange pushConstant{};
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(glm::mat4) + sizeof(int); // Model Matrix + Texture Index

		VkPipelineLayoutCreateInfo pipelineLayoutCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(layouts.size());
		pipelineLayoutCI.pSetLayouts = layouts.data();
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstant;

		if (vkCreatePipelineLayout(m_context->device(), &pipelineLayoutCI, nullptr, &m_defaultLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout!");
		}
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

		// Set the new swapchain as current
		m_swapchain = std::move(newSwapchain);

		m_context->setSwapchainFormat(m_swapchain->getFormat());
		m_context->setDepthFormat(VK_FORMAT_D32_SFLOAT);

		m_depthImage = std::make_unique<VulkanImage>(
			*m_context,
			m_swapchain->getExtent(),
			VK_FORMAT_D32_SFLOAT,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		);


		if (m_renderGraph)
		{
			m_renderGraph->clearExternalResources();

			uint32_t startIdx = 0;
			m_renderGraph->importImage("BackBuffer", m_swapchain->getImageWrapper(startIdx));
			m_renderGraph->importImage("DepthBuffer", m_depthImage.get());
			m_renderGraph->compile(m_renderGraphCompileConfig);
		}
		spdlog::info("Recreating Swapchain: Finished successfully");

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
		auto& scene = SceneManager::getActiveScene();
		m_globalUboData.projection = ctx.projectionMatrix;
		m_globalUboData.view = ctx.viewMatrix;
		m_globalUboData.cameraPos = glm::vec4(ctx.cameraPosition, 1.0f);
		m_globalUboData.time = ctx.totalTime;
		m_globalUboData.deltaTime = ctx.deltaTime;
		m_globalUboData.skyboxIntensity = scene.getSkyboxIntensity();

		m_globalUboBuffers[m_currentFrameIndex]->writeToBuffer(&m_globalUboData);
	}

	void VulkanRenderer::updateBindlessTextures(const std::vector<BindlessUpdateRequest>& updates)
	{
		if (updates.empty()) return;

		DescriptorWriter writer;
		std::vector<VkDescriptorImageInfo> infoBatch;
		infoBatch.reserve(updates.size());

		for (const auto& req : updates)
		{
			infoBatch.push_back(req.info);
			writer.writeImage(
				0,
				&infoBatch.back(),
				1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				req.slot
			);
		}

		writer.updateSet(*m_context, m_bindlessDescriptorSet);
	}

	void VulkanRenderer::updateBindlessTextures(uint32_t slot, VkDescriptorImageInfo& imageInfo)
	{
		DescriptorWriter writer;
		writer.writeImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, slot);
		writer.updateSet(*m_context, m_bindlessDescriptorSet);
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
		m_descriptorManagers.clear();
		m_globalUboBuffers.clear();

		if (m_bindlessPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_context->device(), m_bindlessPool, nullptr);
		if (m_bindlessLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_bindlessLayout, nullptr);
		if (m_computeStorageLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_computeStorageLayout, nullptr);
		
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

