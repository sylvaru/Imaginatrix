#pragma once
#include <string>
#include <memory>
#include <vector>
#include "window_i.h"
#include "input_i.h"
#include "layer_i.h"
#include "renderer_i.h"
#include "platform/glfw_platform.h"


namespace ix
{

	struct EngineSpecification
	{
		std::string name = "Imaginatrix Engine";
		WindowSpecification windowSpec;
		RendererAPI api = RendererAPI::Vulkan;
	};
	class Engine
	{
	public:
		Engine(const EngineSpecification spec);
		~Engine();

		void init();
		void run();

		template<typename T, typename... Args>
		void pushLayer(Args&&... args) {
			auto layer = std::make_shared<T>(std::forward<Args>(args)...);
			m_layers.push_back(layer);
			layer->onAttach();
		}

		void setupInputCallbacks();
		void processInput();
		void toggleCursorLock();
	private:
		std::vector<std::shared_ptr<Layer_I>> m_layers;
		std::unique_ptr<GlfwPlatform> m_platform;
		std::unique_ptr<Renderer_I> m_renderer;
		Window_I& m_window;
		Input_I& m_input;

		bool m_cursorLocked = true;

	};
}