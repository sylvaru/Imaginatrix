#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix
{
    class VulkanPipeline;

    class ImGuiPass : public RenderGraphPass_I
    {
    public:
        ImGuiPass(const std::string& name);
        virtual ~ImGuiPass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;


    private:
        VulkanPipeline* m_cachedPipeline = nullptr;
    };
}
