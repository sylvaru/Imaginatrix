// vk_pipeline_manager.cpp
#include "common/engine_pch.h"
#include "vk_pipeline_manager.h"
#include "vk_pipeline.h"
#include "vk_context.h"



namespace ix
{

	size_t PipelineHasher::operator() (const PipelineState& s) const
	{
		size_t seed{};
		auto hash_combine = [&seed](size_t v)
			{
				seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			};

		hash_combine(static_cast<size_t>(s.topology));
		hash_combine(static_cast<size_t>(s.polygonMode));
		hash_combine(static_cast<size_t>(s.cullMode));
		hash_combine(static_cast<size_t>(s.depthTest));
		hash_combine(static_cast<size_t>(s.depthAttachmentFormat));

		for (const auto& format : s.colorAttachmentFormats)
		{
			hash_combine(static_cast<size_t>(format));
		}

		return seed;
	}


	VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
		: m_context(context) {}

	VulkanPipelineManager::~VulkanPipelineManager() = default;

	// Get pipeline by name
	VulkanPipeline* VulkanPipelineManager::getGraphicsPipeline(const std::string& name) const {
		auto it = m_namedCache.find(name);
		return (it != m_namedCache.end()) ? it->second.get() : nullptr;
	}

	// Get pipeline by state
	VulkanPipeline* VulkanPipelineManager::getGraphicsPipeline(const PipelineState& state) const {
		auto it = m_stateCache.find(state); // Automatically uses PipelineHasher here
		return (it != m_stateCache.end()) ? it->second.get() : nullptr;
	}
	
	VulkanPipeline* VulkanPipelineManager::createGraphicsPipeline(
		const std::string& name,
		const std::string& vertPath,
		const std::string& fragPath,
		const PipelineState& state,
		VkPipelineLayout layout)
	{
		if (auto* existing = getGraphicsPipeline(name)) return existing;

		auto pipeline = std::make_shared<VulkanPipeline>(m_context, vertPath, fragPath, state, layout);

		// Store in the Named Cache
		m_namedCache[name] = pipeline;

		// Store in the State Cache
		m_stateCache[state] = pipeline;
		return pipeline.get();
	}

	void VulkanPipelineManager::clearCache()
	{
		m_stateCache.clear();
		m_namedCache.clear();
	}

}