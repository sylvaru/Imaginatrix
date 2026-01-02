// engine.cpp
#include "common/engine_pch.h"
#include "engine.h"
#include "global_common/ix_global_pods.h"
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
		if (s_instance) { spdlog::error("Engine instance already exists!"); }
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
		auto* vkContext = static_cast <VulkanContext*>(m_renderer->getAPIContext()); // Subject to change
		AssetManager::get().init(vkContext);

		m_renderer->init();

		SceneManager::init();

		for (size_t i{}; i < m_layers.size(); i++) m_layers[i]->onAttach();

		m_renderer->compileRenderGraph();

	}

	void Engine::run()
	{
		spdlog::info("ix::Engine::run() ... ");

		using clock = std::chrono::high_resolution_clock;
		auto lastTime = clock::now();
		double accumulator = 0.0;
		const double dt = 1.0 / 144.0; // Fixed delta time

		while (!m_window.isWindowShouldClose())
		{
			auto currentTime = clock::now();
			double frameTime = std::chrono::duration<double>(currentTime - lastTime).count();
			lastTime = currentTime;
			if (frameTime > 0.25) frameTime = 0.25; // prevents spiral
			accumulator += frameTime;

			if (frameTime > 0.0)
			{
				float currentFPS = static_cast<float>(1.0 / frameTime);

				// Smooth the internal value constantly
				m_smoothedFPS = (m_smoothedFPS * 0.99f) + (currentFPS * 0.01f);

				// Only update the 'display' FPS member every 0.5 seconds
				m_fpsTimer += frameTime;
				if (m_fpsTimer >= 0.5)
				{
					m_lastFrameFPS = m_smoothedFPS;
					m_fpsTimer = 0.0;
				}
			}

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
			auto camera = SceneManager::getActiveCameraMatrices(aspect);

			SceneView view{};
			view.deltaTime = static_cast<float>(frameTime);
			// TODO: Add total time
			view.viewMatrix = camera.getView();
			view.projectionMatrix = camera.getProj();
			view.clusterProjection = camera.getClusterProj();
			view.cameraPosition = camera.getPos();

			FrameContext frameCtx;
			if (m_renderer->beginFrame(frameCtx, view))
			{ 

				float alpha = static_cast<float>(accumulator / dt);

				for (auto& layer : m_layers) layer->onUpdate(static_cast<float>(frameTime));

				m_renderer->render(frameCtx, view);

				for (auto& layer : m_layers) layer->onRender(alpha);

				m_renderer->endFrame(frameCtx);
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