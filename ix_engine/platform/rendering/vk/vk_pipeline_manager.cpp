// vk_pipeline_manager.cpp
#include "common/engine_pch.h"
#include "vk_pipeline_manager.h"
#include "vk_context.h"



namespace ix
{

	VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
		: m_context(context) {
	}

	VulkanPipelineManager::~VulkanPipelineManager()
	{
		clearCache();
	}

	size_t PipelineHasher::operator() (const PipelineState& s) const
	{
		size_t seed{};
		auto hash_combine = [&seed](size_t v) {
			seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			};

		hash_combine(static_cast<size_t>(s.topology));
		hash_combine(static_cast<size_t>(s.polygonMode));
		hash_combine(static_cast<size_t>(s.cullMode));
		hash_combine(static_cast<size_t>(s.frontFace));
		hash_combine(static_cast<size_t>(s.depthTest));
		hash_combine(static_cast<size_t>(s.depthWrite));
		hash_combine(static_cast<size_t>(s.depthCompareOp));
		hash_combine(static_cast<size_t>(s.depthAttachmentFormat));
		hash_combine(static_cast<size_t>(s.isProcedural));

		for (const auto& format : s.colorAttachmentFormats) {
			hash_combine(static_cast<size_t>(format));
		}

		return seed;
	}
	
	VulkanPipeline* VulkanPipelineManager::createGraphicsPipeline(
		const std::string& name,
		const std::string& vertPath,
		const std::string& fragPath,
		const PipelineState& state,
		VkPipelineLayout layout)
	{
		VkPipelineLayout finalLayout = (layout == VK_NULL_HANDLE) ? m_defaultLayout : layout;

		PipelineDefinition def{ name, vertPath, fragPath, state, finalLayout };

		// Check if we already have this definition to avoid duplicates
		auto it = std::find_if(m_definitions.begin(), m_definitions.end(),
			[&](const PipelineDefinition& d) { return d.name == name; });

		if (it == m_definitions.end()) {
			m_definitions.push_back(def);
		}

		return bakePipeline(def);
	}

	VulkanPipeline* VulkanPipelineManager::bakePipeline(const PipelineDefinition& def)
	{
		PipelineState hardwareState = def.baseState;
		hardwareState.colorAttachmentFormats = { m_context.getSwapchainFormat() };
		hardwareState.depthAttachmentFormat = m_context.getDepthFormat();

		auto pipeline = std::make_shared<VulkanPipeline>(
			m_context, def.vertPath, def.fragPath, hardwareState, def.layout);

		m_namedCache[def.name] = pipeline;
		m_stateCache[hardwareState] = pipeline;

		return pipeline.get();
	}

	VulkanComputePipeline* VulkanPipelineManager::createComputePipeline(
		const std::string& name,
		const std::string& shaderPath,
		VkPipelineLayout layout)
	{

		VkPipelineLayout finalLayout = (layout == VK_NULL_HANDLE) ? m_defaultLayout : layout;

		auto pipeline = std::make_shared<VulkanComputePipeline>(m_context, shaderPath, finalLayout);

		m_computeCache[name] = pipeline;

		spdlog::info("VulkanPipelineManager: Baked compute pipeline '{}'", name);
		return pipeline.get();
	}


	// Get pipeline by name
	VulkanPipeline* VulkanPipelineManager::getGraphicsPipeline(const std::string& name) const
	{
		auto it = m_namedCache.find(name);
		return (it != m_namedCache.end()) ? it->second.get() : nullptr;
	}

	// Get pipeline by state
	VulkanPipeline* VulkanPipelineManager::getGraphicsPipeline(PipelineState requestedState)
	{
		// Inject the hardware-specific formats the Pass doesn't know
		requestedState.colorAttachmentFormats = { m_context.getSwapchainFormat() };
		requestedState.depthAttachmentFormat = m_context.getDepthFormat();

		// Perform the hash lookup with the fully qualified state
		auto it = m_stateCache.find(requestedState);
		if (it != m_stateCache.end()) {
			return it->second.get();
		}

		spdlog::error("VulkanPipelineManager: No PSO found for the requested state hash!");
		return nullptr;
	}

	VulkanComputePipeline* VulkanPipelineManager::getComputePipeline(const std::string& name) {
		auto it = m_computeCache.find(name);
		if (it != m_computeCache.end()) {
			return it->second.get();
		}
		spdlog::error("VulkanPipelineManager: Compute pipeline '{}' not found!", name);
		return nullptr;
	}

	void VulkanPipelineManager::reloadPipelines() 
	{
		vkDeviceWaitIdle(m_context.device());

		m_stateCache.clear();
		m_namedCache.clear();

		for (auto& def : m_definitions) {
			bakePipeline(def);
		}

		spdlog::info("VulkanPipelineManager: Re-baked {} pipelines", m_definitions.size());
	}

	VkCullModeFlags VulkanPipelineManager::parseCullMode(const std::string& mode)
	{
		if (mode == "BACK" || mode == "back") return VK_CULL_MODE_BACK_BIT;
		if (mode == "FRONT" || mode == "front") return VK_CULL_MODE_FRONT_BIT;
		if (mode == "NONE" || mode == "none") return VK_CULL_MODE_NONE;
		return VK_CULL_MODE_BACK_BIT;
	}

	VkPrimitiveTopology VulkanPipelineManager::parseTopology(const std::string& topo)
	{
		if (topo == "TRIANGLE_LIST") return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		if (topo == "TRIANGLE_STRIP") return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}


	void VulkanPipelineManager::clearCache()
	{
		m_stateCache.clear();
		m_namedCache.clear();

		for (auto layout : m_createdLayouts)
		{
			if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_context.device(), layout, nullptr);
		}
		m_definitions.clear();
		m_createdLayouts.clear();
	}

}