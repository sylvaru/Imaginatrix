// ix_engine/platform/rendering/rendering_api.h
#pragma once
#include <memory>
#include "renderer_i.h"
#include "window_i.h"

namespace ix
{
	std::unique_ptr<Renderer_I>
		createRenderer(Window_I& window, RendererAPI api);
}