#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace ix
{
    class RenderGraphBuilder;
    class RenderGraphRegistry;
    struct FrameContext;
    struct RenderState;

    using RGResourceHandle = uint32_t;
    const RGResourceHandle INVALID_RESOURCE = 0xFFFFFFFF;

    enum class RGResourceType { Image, Buffer };

    struct RGResourceview {
        RGResourceHandle handle;
        RGResourceType type;
    };

    enum class PassType {
        Graphics,
        Compute
    };

    class RenderGraphPass_I {
    public:
        RenderGraphPass_I(const std::string& name) : m_name(name) {}
        virtual ~RenderGraphPass_I() = default;

        virtual void setup(RenderGraphBuilder& builder) = 0;

        virtual void execute(const RenderState& state, RenderGraphRegistry& registry) = 0;

        const std::string& getName() const { return m_name; }

        virtual PassType getPassType() const { return PassType::Graphics; }

    private:
        std::string m_name;
    };
}