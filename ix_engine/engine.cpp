// engine.cpp
#include "common/engine_pch.h"
#include "engine.h"
#include "global_common/ix_gpu_types.h"
#include "core/scene_manager.h"
#include "platform/rendering/rendering_api.h"
#include "platform/glfw_platform.h"

#include "input_i.h"
#include "layer_i.h"

namespace ix
{

	Engine* Engine::s_instance = nullptr;
	Engine::Engine(const EngineSpecification spec)
		: m_platform(std::make_unique<GlfwPlatform>(spec.windowSpec, spec.name))
		, m_window(*m_platform)
		, m_input(*m_platform)
	{
		if (s_instance) { spdlog::error("Engine instance alredy exists!"); }
		s_instance = this;

		m_renderer = createRenderer(m_window, spec.api);
	}

	Engine::~Engine() { shutdown();	}

	void Engine::init()
	{
		spdlog::info("ix::Engine::init() ... ");

		m_window.lockCursor(m_cursorLocked);
		setupInputCallbacks();

		// Init core systems
		m_renderer->init();

		auto* vkContext = static_cast <VulkanContext*>(m_renderer->getAPIContext());
		if (!vkContext) {
			spdlog::critical("Engine: Renderer failed to provide a valid Vulkan Context!");
			return;
		}
		AssetManager::get().init(vkContext);

		SceneManager::init();
	}

	void Engine::run()
	{
		spdlog::info("ix::Engine::run() ... ");

		using clock = std::chrono::high_resolution_clock;
		auto lastTime = clock::now();
		double accumulator = 0.0;

		// Fixed delta time
		const double dt = 1.0 / 144.0;

		while (!m_window.isWindowShouldClose())
		{
			auto currentTime = clock::now();
			double frameTime = std::chrono::duration<double>(currentTime - lastTime).count();

			if (frameTime > 0.25) frameTime = 0.25; // prevents spiral

			lastTime = currentTime;
			accumulator += frameTime;

			m_window.pollEvents();

			while (accumulator >= dt)
			{
				// Process System-level Input
				processInput();

				for (auto& layer : m_layers) {
					layer->onFixedUpdate(static_cast<float>(dt));
				}

				accumulator -= dt;
			}
			auto extent = m_renderer->getSwapchainExtent();
			float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
			auto activeCameraMatrices = SceneManager::getActiveCameraMatrices(aspect);

			FrameContext ctx;
			ctx.deltaTime = static_cast<float>(frameTime);
			//ctx.totalTime = /* tracked total time */;
			ctx.viewMatrix = activeCameraMatrices.getView();
			ctx.projectionMatrix = activeCameraMatrices.getProj();
			ctx.assetManager = &AssetManager::get();


			if (m_renderer->beginFrame(ctx)) 
			{ 

				float alpha = static_cast<float>(accumulator / dt);
				for (auto& layer : m_layers) {
					layer->onUpdate(static_cast<float>(frameTime));
				}

				m_renderer->render(ctx);

				for (auto& layer : m_layers) {
					layer->onRender(alpha);
				}

				m_renderer->endFrame(ctx);
			}
			
		}
	}

	void Engine::setupInputCallbacks()
	{
		m_window.setKeyCallback([this](IxKey key, int scacode, KeyAction action, int mods) {
			if (key == IxKey::LEFT_ALT && action == KeyAction::PRESS) 
			{
				toggleCursorLock();
			}
			});
	}

	void Engine::processInput()
	{
		if (m_window.isKeyPressed(IxKey::ESCAPE))
		{
			m_window.requestWindowClose();
		}
	}

	void Engine::toggleCursorLock()
	{
		m_cursorLocked = !m_cursorLocked;
		m_window.lockCursor(m_cursorLocked);
	}

	void Engine::shutdown()
	{
		if (m_renderer) m_renderer->waitIdle(); // wait for gpu to finish work
		m_layers.clear();
		SceneManager::shutdown();
		AssetManager::get().clearAssetCache();
		if (m_renderer)	m_renderer->shutdown();
		s_instance = nullptr;
	}
	
}