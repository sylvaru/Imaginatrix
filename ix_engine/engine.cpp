// engine.cpp
#include "common/engine_pch.h"
#include "engine.h"
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

	Engine::~Engine()
	{
		SceneManager::shutdown();
		s_instance = nullptr;
	}

	void Engine::init()
	{
		spdlog::info("ix::Engine::init() ... ");

		m_window.lockCursor(m_cursorLocked);
		setupInputCallbacks();
		
		// Init core systems
		m_renderer->init();
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
			m_renderer->beginFrame();

			float alpha = static_cast<float>(accumulator / dt);
			for (auto& layer : m_layers) {
				layer->onUpdate(static_cast<float>(frameTime));
			}

			for (auto& layer : m_layers) {
				layer->onRender(alpha);
			}

			m_renderer->endFrame();
	
		}
	}

	void Engine::setupInputCallbacks()
	{
		m_window.setKeyCallback([this](IxKey key, int scacode, KeyAction action, int mods) {
			if (key == IxKey::LEFT_ALT && action == KeyAction::PRESS) {
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
	
}