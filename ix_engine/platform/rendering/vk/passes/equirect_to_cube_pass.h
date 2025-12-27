// compute_pass.h
#pragma once
#include "render_graph_pass_i.h"
#include <vulkan/vulkan.h>

namespace ix
{
    // Runs once (at startup or when skybox changes).
    class EquirectToCubemapPass : public RenderGraphPass_I
    {
    public:
        EquirectToCubemapPass(const std::string& name);
        virtual ~EquirectToCubemapPass() = default;

        void setup(RenderGraphBuilder& builder) override;

        void execute(const RenderState& state, RenderGraphRegistry& registry) override;

    };
}