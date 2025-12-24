// ix_interface/renderer_i.h
#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ix 
{ 
    class Scene; 
    struct FrameContext;
    class VulkanSwapchain;
}

namespace ix {

    enum class RendererAPI { None, Vulkan, OpenGL };

    struct RenderStats {
        uint32_t drawCalls = 0;
        uint32_t triangleCount = 0;
    };

    struct RenderExtent {
        uint32_t width;
        uint32_t height;
    };

    struct Window_I;

    class Renderer_I {
    public:
        virtual ~Renderer_I() = default;

        // Lifecycle
        virtual void init() = 0;
        virtual void shutdown() = 0;
        virtual void* getAPIContext() = 0;
        virtual void waitIdle() {}

        // Window resizing
        virtual void onResize(uint32_t width, uint32_t height) = 0;

        // The Frame Cycle
        virtual bool beginFrame(FrameContext& ctx) = 0;
        virtual void endFrame(const FrameContext& ctx) = 0;
        virtual void render(const FrameContext& ctx) = 0;

        virtual void loadPipelines(const nlohmann::json& json) {}
        virtual RenderExtent getSwapchainExtent() const = 0;
        virtual VulkanSwapchain* getSwapchain() = 0;
        virtual uint32_t getCurrentImageIndex() const = 0;

    };
}