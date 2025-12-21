#include "common/engine_pch.h"
#include "scene_manager.h"

namespace ix
{
	SceneManager* SceneManager::s_instance = nullptr;
	std::unique_ptr<Scene> SceneManager::s_activeScene = nullptr;

	void SceneManager::init()
	{
		s_instance = new SceneManager();
		s_instance->s_activeScene = std::make_unique<Scene>();
	}
	void SceneManager::load(const std::string& fileName)
	{
		std::string path = std::string(PROJECT_ROOT_DIR) +
			"/sandbox_game/res/scenes/" + fileName + ".json";

		if (!std::filesystem::exists(path)) {
			spdlog::error("Failed to load scene: File not found at {}", path);
			return;
		}

		auto newScene = std::make_unique<Scene>();
		
		// Swap scenes
		s_instance->s_activeScene = std::move(newScene);
		spdlog::info("Scene loaded: {}", fileName);
	}

	void SceneManager::shutdown()
	{
		s_activeScene.reset();
		delete s_instance;
		s_instance = nullptr;
	}
}