// skybox_pass.h
#pragma once
#include "render_graph_pass_i.h"


namespace ix
{
	class VulkanPipeline;

	class SkyboxPass : public RenderGraphPass_I
	{
	public:
		SkyboxPass(const std::string& name);
		virtual ~SkyboxPass() = default;

		void setup(RenderGraphBuilder& builder) override;

		void execute(const RenderState& state, RenderGraphRegistry& registry) override;

	private:
		VulkanPipeline* m_cachedPipeline = nullptr;
	};
}

