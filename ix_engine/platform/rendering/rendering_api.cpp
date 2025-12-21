// ix_engine/platform/rendering/rendering_api.cpp
#include "platform/rendering/rendering_api.h"
#include "platform/rendering/vk/vk_renderer.h"

namespace ix
{
	std::unique_ptr<Renderer_I>
		createRenderer(Window_I& window, RendererAPI api)
	{
		switch (api)
		{
		case RendererAPI::Vulkan: return std::make_unique<VulkanRenderer>(window);
		case RendererAPI::OpenGL: spdlog::error("OpenGL not implemented yet!");
			return nullptr;
		default:
			return nullptr;
		}
	}
}


