// vk_renderer.cpp
#include "common/engine_pch.h"
#include "vk_renderer.h"
#include "common/handles.h"
#include "window_i.h"

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
#include "platform/rendering/vk/passes/equirect_to_cube_pass.h"
#include "platform/rendering/vk/passes/compute_culling_pass.h"
#include "platform/rendering/vk/passes/imgui_pass.h"


#include "core/asset_manager.h"
#include "core/scene_manager.h"
#include "core/components.h"


#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>


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
		createDescriptorAndPipelineLayouts();

		// Initialize Global UBO Buffers
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

		// Initialize Instance Database (Input)
		m_instanceBuffer = std::make_unique<VulkanBuffer>(
			*m_context,
			sizeof(GPUInstanceData) * 3000,
			1,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		m_instanceBuffer->map();

		// Initialize Culled Instance Buffer (Commands + Filtered Data)
		const uint32_t MAX_BATCHES = 16;
		const VkDeviceSize commandHeaderSize = 1024; // 16 * sizeof(GPUIndirectCommand)

		m_culledInstanceBuffer = std::make_unique<VulkanBuffer>(
			*m_context,
			commandHeaderSize + (sizeof(GPUInstanceData) * 3000),
			1,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY
		);

		// Initialize Bindless Textures
		m_bindlessPool = m_descriptorManagers[0]->createBindlessPool(1, 1000);
		m_descriptorManagers[0]->allocateBindless(m_bindlessPool, &m_bindlessDescriptorSet, m_bindlessLayout, 1000);

		// PRE-ALLOCATE PER-FRAME DESCRIPTORS
		m_frameDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto& sets = m_frameDescriptorSets[i];
			auto& mgr = m_descriptorManagers[i];

			// Set 0: Global UBO (Binding 0)
			mgr->allocate(&sets.globalSet, m_globalDescriptorLayout);
			auto uboInfo = m_globalUboBuffers[i]->descriptorInfo();
			DescriptorWriter()
				.writeBuffer(0, &uboInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				.updateSet(*m_context, sets.globalSet);

			// Set 2 (Forward)
			mgr->allocate(&sets.instanceSet, m_instanceDescriptorLayout);
			VkDescriptorBufferInfo culledDataInfo = m_culledInstanceBuffer->descriptorInfo();
			culledDataInfo.offset = commandHeaderSize; // The 1024 offset
			culledDataInfo.range = VK_WHOLE_SIZE;

			DescriptorWriter()
				.writeBuffer(2, &culledDataInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				.updateSet(*m_context, sets.instanceSet);

			// Set 2 (Compute)
			mgr->allocate(&sets.cullingSet, m_cullingDescriptorLayout);
			auto inputDbInfo = m_instanceBuffer->descriptorInfo();

			// Binding 1 (Commands)
			VkDescriptorBufferInfo cmdInfo = m_culledInstanceBuffer->descriptorInfo();
			cmdInfo.offset = 0;
			cmdInfo.range = commandHeaderSize;

			// Binding 2 (CulledInstances)
			VkDescriptorBufferInfo culledOutInfo = m_culledInstanceBuffer->descriptorInfo();
			culledOutInfo.offset = commandHeaderSize;
			culledOutInfo.range = VK_WHOLE_SIZE;

			DescriptorWriter()
				.writeBuffer(0, &inputDbInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)   // Binding 0
				.writeBuffer(1, &cmdInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)       // Binding 1
				.writeBuffer(2, &culledOutInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) // Binding 2
				.updateSet(*m_context, sets.cullingSet);
		}

		spdlog::info("Vulkan Renderer: Initialized. Descriptors baked for {} frames.", MAX_FRAMES_IN_FLIGHT);
	}

	void VulkanRenderer::compileRenderGraph()
	{
		auto forwardPass = std::make_unique<ForwardPass>("MainForward");
		auto equirectToCubePass = std::make_unique<EquirectToCubemapPass>("ComputePass");
		auto computeCullingPass = std::make_unique<ComputeCullingPass>("ComputeCulling");
		auto skyboxPass = std::make_unique<SkyboxPass>("SkyboxPass");
		auto imguiPass = std::make_unique<ImGuiPass>("ImGuiPass");

		m_renderGraphCompileConfig.context = m_context.get();
		m_renderGraphCompileConfig.pipelineManager = m_pipelineManager.get();

		m_renderGraph->importBuffer("InstanceDb", m_instanceBuffer.get());
		m_renderGraph->importBuffer("CulledInstances", m_culledInstanceBuffer.get());

		m_renderGraph->addPass(std::move(computeCullingPass));
		m_renderGraph->addPass(std::move(forwardPass));
		m_renderGraph->addPass(std::move(equirectToCubePass));
		m_renderGraph->addPass(std::move(skyboxPass));
		m_renderGraph->addPass(std::move(imguiPass));
		m_renderGraph->compile(m_renderGraphCompileConfig);
	}
	
	bool VulkanRenderer::beginFrame(FrameContext& ctx, const SceneView& view)
	{

		if (m_window.wasWindowResized() || m_needsSwapchainRecreation)
		{
			recreateSwapchain();
			m_window.setWindowResizedFlag(false);
			return false;
		}
		FrameData& frame = getCurrentFrame();

		// Synchronize: Wait for GPU to finish this frame's previous iteration
		vkWaitForFences(m_context->device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

		// Acquire Image
		uint32_t imageIndex;
		VkResult result = m_swapchain->acquireNextImage(frame.imageAvailableSemapohore, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			recreateSwapchain();
			return false;
		}
		m_currentImageIndex = imageIndex;

		// Reset Command Buffer & Fence
		vkResetFences(m_context->device(), 1, &frame.inFlightFence);
		vkResetCommandBuffer(frame.commandBuffer, 0);

		// Update CPU Data (UBO and Instance Database)
		updateGlobalUbo(view);
		updateInstanceBuffer();

		// Start Recording
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("VulkanRenderer: Failed to begin recording command buffer!");
		}

		// Reset Indirect Commands
		std::vector<GPUIndirectCommand> resetCmds;
		resetCmds.reserve(m_renderBatches.size());

		for (const auto& batch : m_renderBatches) {
			VulkanMesh* mesh = AssetManager::get().getMesh(batch.meshHandle);
			GPUIndirectCommand cmd{};
			cmd.indexCount = mesh ? mesh->indexCount : 0;
			cmd.instanceCount = 0;
			cmd.firstIndex = 0;
			cmd.vertexOffset = 0;
			cmd.firstInstance = 0;
			resetCmds.push_back(cmd);
		}

		if (!resetCmds.empty()) {
			vkCmdUpdateBuffer(
				frame.commandBuffer,
				m_culledInstanceBuffer->getBuffer(),
				0,
				resetCmds.size() * sizeof(GPUIndirectCommand),
				resetCmds.data()
			);
		}

		// Barrier: Transfer -> Compute (Ensures reset is finished before culling starts)
		VkBufferMemoryBarrier resetBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		resetBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		resetBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		resetBarrier.buffer = m_culledInstanceBuffer->getBuffer();
		resetBarrier.offset = 0;
		resetBarrier.size = 1024;

		vkCmdPipelineBarrier(
			frame.commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 1, &resetBarrier, 0, nullptr
		);


		// Finalize FrameContext
		auto& bakedSets = m_frameDescriptorSets[m_currentFrameIndex];

		ctx.frameIndex = m_currentFrameIndex;
		ctx.commandBuffer = frame.commandBuffer;

		ctx.globalDescriptorSet = bakedSets.globalSet;
		ctx.instanceDescriptorSet = bakedSets.instanceSet;
		ctx.cullingDescriptorSet = bakedSets.cullingSet;
		ctx.bindlessDescriptorSet = m_bindlessDescriptorSet;

		ctx.instanceCount = m_currentInstanceCount;
		ctx.renderBatches = &m_renderBatches;

		return true;
	}

	void VulkanRenderer::render(const FrameContext& ctx, const SceneView& view)
	{
		updateBindlessTextures(AssetManager::get().takePendingUpdates());

		RenderContext system;
		system.context = m_context.get();
		system.assetManager = &AssetManager::get();
		system.pipelineManager = m_pipelineManager.get();
		system.descriptorManager = m_descriptorManagers[m_currentFrameIndex].get();
		system.computeStorageLayout = m_computeStorageLayout;
		system.computeCullingLayout = m_cullingDescriptorLayout;
	
		RenderState state{ system, ctx, view };

		VulkanImage* currentImg = m_swapchain->getImageWrapper(m_currentImageIndex);
		if (currentImg) 
		{
			m_renderGraph->importImage("BackBuffer", currentImg);
			m_renderGraph->importImage("DepthBuffer", m_depthImage.get());
		}

		m_renderGraph->execute(state);
	}

	void VulkanRenderer::updateInstanceBuffer()
	{
		auto& scene = SceneManager::getActiveScene();
		auto& registry = scene.getRegistry();

		// Use EnTT group for contiguous memory access
		auto group = registry.group<MeshComponent>(entt::get<TransformComponent>);

		// Only do a full rebuild if the scene structure changed
		static size_t lastEntityCount = 0;
		bool needsFullRebuild = (group.size() != lastEntityCount);

		if (needsFullRebuild)
		{
			// Clear caches
			for (auto& [handle, instances] : m_batchMapCache) instances.clear();
			m_cpuInstanceCache.clear();
			m_renderBatches.clear();

			m_cpuInstanceCache.reserve(group.size());

			// Group by MeshHandle
			group.each([&](auto entity, auto& mesh, auto& transform) {
				GPUInstanceData data{};
				data.modelMatrix = transform.getTransform();
				data.textureIndex = AssetManager::get().getTextureBindlessIndex(mesh.textureHandle);
				
				float baseRadius = AssetManager::get().getMeshBoundingRadius(mesh.meshHandle);
				float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
				data.boundingRadius = baseRadius * maxScale;

				m_batchMapCache[mesh.meshHandle].push_back(data);
				});

			// Flatten and assign BatchIDs
			uint32_t currentOffset = 0;
			uint32_t batchIndex = 0;

			for (auto& [meshHandle, instances] : m_batchMapCache)
			{
				if (instances.empty()) continue;

				// Create the metadata for this batch
				RenderBatch batch;
				batch.meshHandle = meshHandle;
				batch.instanceCount = static_cast<uint32_t>(instances.size());
				batch.firstInstance = currentOffset;
				m_renderBatches.push_back(batch);

				// Assign the Batch ID to every instance in this group
				for (auto& inst : instances) {
					inst.batchID = batchIndex;
				}

				// Move into flat GPU-ready vector
				m_cpuInstanceCache.insert(m_cpuInstanceCache.end(), instances.begin(), instances.end());

				currentOffset += batch.instanceCount;
				batchIndex++;
			}

			lastEntityCount = group.size();
		}
		else
		{
			// FAST PATH: Only update matrices for dirty transforms
			uint32_t i = 0;
			group.each([&](auto entity, auto& mesh, auto& transform) {
				// Only update if the object actually moved
				if (transform.dirty) 
				{
					float baseRadius = AssetManager::get().getMeshBoundingRadius(mesh.meshHandle);
					float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
					m_cpuInstanceCache[i].modelMatrix = transform.getTransform();

				}
				i++;
				});
		}

		m_currentInstanceCount = static_cast<uint32_t>(m_cpuInstanceCache.size());

		// Single GPU Upload
		if (m_currentInstanceCount > 0)
		{
			m_instanceBuffer->writeToBuffer(m_cpuInstanceCache.data(), m_currentInstanceCount * sizeof(GPUInstanceData));
		}

		// Logging
		static size_t lastBatchCount = 0;
		static uint32_t lastInstanceCount = 0;
		if (m_renderBatches.size() != lastBatchCount || m_currentInstanceCount != lastInstanceCount) {
			spdlog::info("Batcher: Flattened {} instances into {} unique batches",
				m_currentInstanceCount, m_renderBatches.size());
			lastBatchCount = m_renderBatches.size();
			lastInstanceCount = m_currentInstanceCount;
		}
	}

	void VulkanRenderer::updateGlobalUbo(const SceneView& view)
	{
		auto& scene = SceneManager::getActiveScene();

		// Mapping SceneView (CPU) to GlobalUbo (GPU Layout)
		m_globalUboData.projection = view.projectionMatrix;
		m_globalUboData.view = view.viewMatrix;
		m_globalUboData.cameraPos = glm::vec4(view.cameraPosition, 1.0f);
		m_globalUboData.time = view.totalTime;
		m_globalUboData.deltaTime = view.deltaTime;

		// External scene parameters
		m_globalUboData.skyboxIntensity = scene.getSkyboxIntensity();

		// Commit to the buffer for the current frame
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

			VkPipelineLayout activeLayout = VK_NULL_HANDLE;

			if (type == "compute") 
			{
				if (name == "FrustumCull") 
				{
					activeLayout = m_cullingPipelineLayout;
				}
				else 
				{
					activeLayout = m_computePipelineLayout;
				}
			}
			else if (type == "graphics") {
				activeLayout = m_graphicsPipelineLayout;
			}

			if (activeLayout == VK_NULL_HANDLE) {
				throw std::runtime_error("Pipeline '" + name + "' has no valid layout defined!");
			}

			if (type == "compute") {
				std::string compPath = shaderRoot + entry["compute"].get<std::string>();
				m_pipelineManager->createComputePipeline(name, compPath, activeLayout);
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

				m_pipelineManager->createGraphicsPipeline(name, vertPath, fragPath, state, activeLayout);

				spdlog::info("VulkanRenderer: Created graphics pipeline '{}'", name);
			}
		}
	}

	void VulkanRenderer::createDescriptorAndPipelineLayouts()
	{
		// Set 0: Global UBO
		VkDescriptorSetLayoutBinding globalBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

		VkDescriptorSetLayoutCreateInfo uboCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		uboCI.bindingCount = 1;
		uboCI.pBindings = &globalBinding;
		vkCreateDescriptorSetLayout(m_context->device(), &uboCI, nullptr, &m_globalDescriptorLayout);

		// Set 1: Bindless Textures
		VkDescriptorSetLayoutBinding bindlessBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000,
			VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

		VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		bindingFlags.bindingCount = 1;
		bindingFlags.pBindingFlags = &flags;

		VkDescriptorSetLayoutCreateInfo bindlessCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		bindlessCI.pNext = &bindingFlags;
		bindlessCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		bindlessCI.bindingCount = 1;
		bindlessCI.pBindings = &bindlessBinding;
		vkCreateDescriptorSetLayout(m_context->device(), &bindlessCI, nullptr, &m_bindlessLayout);

		// Set 2: Graphics Instance Data
		VkDescriptorSetLayoutBinding instBinding = { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
		VkDescriptorSetLayoutCreateInfo instCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &instBinding };
		vkCreateDescriptorSetLayout(m_context->device(), &instCI, nullptr, &m_instanceDescriptorLayout);

		// Set 2: Culling Storage (Input + Output)
		std::vector<VkDescriptorSetLayoutBinding> cullBindings = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }, // Input
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }, // Commands
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr }
		};
		VkDescriptorSetLayoutCreateInfo cullLayoutCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		cullLayoutCI.bindingCount = static_cast<uint32_t>(cullBindings.size());
		cullLayoutCI.pBindings = cullBindings.data();
		vkCreateDescriptorSetLayout(m_context->device(), &cullLayoutCI, nullptr, &m_cullingDescriptorLayout);

		// Storage image layout
		VkDescriptorSetLayoutBinding storageImgBinding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
		VkDescriptorSetLayoutCreateInfo storageImgCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		storageImgCI.bindingCount = 1;
		storageImgCI.pBindings = &storageImgBinding;
		vkCreateDescriptorSetLayout(m_context->device(), &storageImgCI, nullptr, &m_computeStorageLayout);


		// Push constant ranges
		// Graphics only (Vertex/Fragment)
		VkPushConstantRange graphicsRange{};
		graphicsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		graphicsRange.offset = 0;
		graphicsRange.size = sizeof(CullingPushConstants); 

		// Compute only
		VkPushConstantRange computeRange{};
		computeRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		computeRange.offset = 0;
		computeRange.size = sizeof(CullingPushConstants);

		// Graphics Layout
		std::vector<VkDescriptorSetLayout> gSets = { m_globalDescriptorLayout, m_bindlessLayout, m_instanceDescriptorLayout };
		VkPipelineLayoutCreateInfo gLayoutCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		gLayoutCI.setLayoutCount = (uint32_t)gSets.size();
		gLayoutCI.pSetLayouts = gSets.data();
		gLayoutCI.pushConstantRangeCount = 1;
		gLayoutCI.pPushConstantRanges = &graphicsRange; // Uses VERTEX | FRAGMENT
		vkCreatePipelineLayout(m_context->device(), &gLayoutCI, nullptr, &m_graphicsPipelineLayout);

		// Culling Layout
		std::vector<VkDescriptorSetLayout> cullingSets = { m_globalDescriptorLayout, m_bindlessLayout, m_cullingDescriptorLayout };
		VkPipelineLayoutCreateInfo cullingCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		cullingCI.setLayoutCount = (uint32_t)cullingSets.size();
		cullingCI.pSetLayouts = cullingSets.data();
		cullingCI.pushConstantRangeCount = 1;
		cullingCI.pPushConstantRanges = &computeRange; // Uses COMPUTE only
		vkCreatePipelineLayout(m_context->device(), &cullingCI, nullptr, &m_cullingPipelineLayout);

		// General Compute Layout
		std::vector<VkDescriptorSetLayout> computeSets = {
			m_globalDescriptorLayout,
			m_bindlessLayout,
			m_computeStorageLayout
		};

		VkPipelineLayoutCreateInfo compLayoutCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		compLayoutCI.setLayoutCount = static_cast<uint32_t>(computeSets.size());
		compLayoutCI.pSetLayouts = computeSets.data();
		compLayoutCI.pushConstantRangeCount = 1; 
		compLayoutCI.pPushConstantRanges = &computeRange;

		if (vkCreatePipelineLayout(m_context->device(), &compLayoutCI, nullptr, &m_computePipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create general compute pipeline layout!");
		}
	}


	void VulkanRenderer::recreateSwapchain()
	{
		spdlog::info("Recreating Swapchain (VSync: {})...", m_vsync ? "ON" : "OFF");
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
			m_swapchain.get(),
			m_vsync
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

			m_renderGraph->importBuffer("InstanceDb", m_instanceBuffer.get());
			m_renderGraph->importBuffer("CulledInstances", m_culledInstanceBuffer.get());

			m_renderGraph->compile(m_renderGraphCompileConfig);
		}
		m_needsSwapchainRecreation = false;
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
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

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

	void VulkanRenderer::setupImGui()
	{
		if (m_imguiPool == VK_NULL_HANDLE) 
		{
			VkDescriptorPoolSize pool_sizes[] = {
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
			};
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 100;
			pool_info.poolSizeCount = std::size(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			vkCreateDescriptorPool(m_context->device(), &pool_info, nullptr, &m_imguiPool);
		}

		// Initialize ImGui Vulkan Implementation
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = m_instance->get();
		init_info.PhysicalDevice = m_context->physicalDevice();
		init_info.Device = m_context->device();
		init_info.QueueFamily = m_context->getGraphicsFamily();
		init_info.Queue = m_context->graphicsQueue();
		init_info.DescriptorPool = m_imguiPool;
		init_info.MinImageCount = m_swapchain->getImageCount();
		init_info.ImageCount = m_swapchain->getImageCount();
		init_info.UseDynamicRendering = true;

		VkFormat swapchainFormat = m_swapchain->getFormat();
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;

		ImGui_ImplVulkan_Init(&init_info);


	}


	void VulkanRenderer::shutdown()
	{
		if (!m_context) return;

		// Shutdown imgui
		ImGui_ImplGlfw_Shutdown();
		ImGui_ImplVulkan_Shutdown();
		ImGui::DestroyContext();
		if (m_imguiPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_context->device(), m_imguiPool, nullptr);
		

		// Cleanup / reset owned resources
		if (m_pipelineManager) 
		{
			m_pipelineManager->clearCache();
			m_pipelineManager.reset();
		}

		if (m_graphicsPipelineLayout != VK_NULL_HANDLE) 
		{
			vkDestroyPipelineLayout(m_context->device(), m_graphicsPipelineLayout, nullptr);
			m_graphicsPipelineLayout = VK_NULL_HANDLE;
		}
		if (m_computePipelineLayout != VK_NULL_HANDLE) 
		{
			vkDestroyPipelineLayout(m_context->device(), m_computePipelineLayout, nullptr);
			m_computePipelineLayout = VK_NULL_HANDLE;
		}
		if (m_cullingPipelineLayout != VK_NULL_HANDLE) 
		{
			vkDestroyPipelineLayout(m_context->device(), m_cullingPipelineLayout, nullptr);
			m_cullingPipelineLayout = VK_NULL_HANDLE;
		}

		if (m_renderGraph) 
		{
			m_renderGraph->clearExternalResources();
			m_renderGraph.reset();
		}
		m_depthImage.reset();
		m_descriptorManagers.clear();
		m_globalUboBuffers.clear();
		m_instanceBuffer.reset();
		m_culledInstanceBuffer.reset();
		m_frameDescriptorSets.clear();


		if (m_bindlessPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_context->device(), m_bindlessPool, nullptr);
		if (m_bindlessLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_bindlessLayout, nullptr);
		if (m_computeStorageLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_computeStorageLayout, nullptr);
		if (m_cullingDescriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_cullingDescriptorLayout, nullptr);
		if (m_instanceDescriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_instanceDescriptorLayout, nullptr);
		if (m_globalDescriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_context->device(), m_globalDescriptorLayout, nullptr);


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

