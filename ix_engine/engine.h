// engine.h
#pragma once
#include <string>
#include <memory>
#include <vector>

#include "renderer_i.h"
#include "window_i.h"
#include "core/asset_manager.h"

namespace ix 
{
	class Input_I;
	class Layer_I;
	class GlfwPlatform;


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
		}

		static Engine& get() { return *s_instance; }
		static AssetManager& getAssetManager() { return AssetManager::get(); }
		Renderer_I& getRenderer() { return *m_renderer; }
		Input_I& getInput() { return m_input; }

		void setupInputCallbacks();
		void processInput();
		void toggleCursorLock();
	private:
		void shutdown();
		static Engine* s_instance;
		std::vector<std::shared_ptr<Layer_I>> m_layers;
		std::unique_ptr<GlfwPlatform> m_platform;
		std::unique_ptr<Renderer_I> m_renderer;
		Window_I& m_window;
		Input_I& m_input;

		bool m_cursorLocked = true;

	};
}