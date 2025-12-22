// ix_interface/renderer_i.h
#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ix 
{ 
    class Scene; 
}

namespace ix {

    enum class RendererAPI { None, Vulkan, OpenGL };

    struct RenderStats {
        uint32_t drawCalls = 0;
        uint32_t triangleCount = 0;
    };

    struct Window_I;

    class Renderer_I {
    public:
        virtual ~Renderer_I() = default;

        // Lifecycle
        virtual void init() = 0;
        virtual void shutdown() = 0;

        // Window resizing
        virtual void onResize(uint32_t width, uint32_t height) = 0;

        // The Frame Cycle
        virtual void beginFrame() = 0;
        virtual void endFrame() = 0;

        // The Engine passes the entire scene, and the renderer queries the Registry
        virtual void submitScene(Scene& scene, float alpha) = 0;

        virtual void loadPipelines(const nlohmann::json& json) {}

    };
}